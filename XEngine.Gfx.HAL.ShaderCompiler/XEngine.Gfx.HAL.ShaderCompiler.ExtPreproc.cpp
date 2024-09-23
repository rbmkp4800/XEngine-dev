#include <XLib.Fmt.h>

#include "XEngine.Gfx.HAL.ShaderCompiler.h"

#include "XEngine.Gfx.HAL.ShaderCompiler.ExtPreproc.h"

using namespace XLib;
using namespace XEngine::Gfx::HAL;
using namespace XEngine::Gfx::HAL::ShaderCompiler;
using namespace XEngine::Gfx::HAL::ShaderCompiler::ExtPreproc;

namespace XEngine::Gfx::HAL::ShaderCompiler::ExtPreproc
{
	enum class LexemeType : uint8
	{
		EndOfFile = 0,

		Dot = '.',
		LeftParen = '(',
		RightParen = ')',
		LeftAngleBracket = '<',
		RightAngleBracket = '>',
		LeftSquareBracket = '[',
		RightSquareBracket = ']',
		Semicolon = ';',
		Comma = ',',

		Identifier = 128,
		NumericLiteral,
		StringLiteral,

		DoubleColon,
	};

	enum class AttributeType : uint8
	{
		None = 0,
		Binding,
		//ExportCBLayout,
	};

	enum class ResourceType : uint8
	{
		Undefined = 0,
		ConstantBuffer,
		Buffer,
		RWBuffer,
		Texture,
		RWTexture,
		RaytracingAccelerationStructure,
	};

	enum class BindingType : uint8
	{
		Undefined = 0,
		Resource,
		Constant,
		StaticSampler,
	};


	struct SourceLocation
	{
		StringViewASCII filename;
		uint32 lineNumber;
		uint32 columnNumber;
	};

	struct Attribute
	{
		union
		{
			struct
			{
				StringViewASCII rootName;
				StringViewASCII nestedName;

				SourceLocation rootNameLocation;
				SourceLocation nestedNameLocation;
			} binding;
		};

		AttributeType type;
	};

	struct BindingInfo
	{
		union
		{
			struct
			{
				uint32 shaderRegister;
				ResourceType type;
				bool allowArray;
			} resource;

			struct
			{

			} constant;

			struct
			{
				uint32 shaderRegister;
			} staticSampler;
		};

		BindingType type;
	};


	class OutputCollector : public NonCopyable
	{
	private:
		VirtualStringWriter outputWriter;

	public:
		inline OutputCollector(VirtualStringRefASCII output) : outputWriter(output) {}
		~OutputCollector() = default;

		template <typename ... FmtArgs>
		inline void appendMessageFmt(SourceLocation sourceLocation, const FmtArgs& ... fmtArgs)
		{
			FmtPrint(outputWriter,
				sourceLocation.filename, ':', sourceLocation.lineNumber, ':', sourceLocation.columnNumber,
				": XE ext shader preprocessor: error: ", fmtArgs ..., '\n');
		}
	};

	class CharsReader : public LineColumnTrackingCharStreamReaderWrapper<MemoryCharStreamReader>
	{
	private:
		using Base = LineColumnTrackingCharStreamReaderWrapper<MemoryCharStreamReader>;

	private:
		MemoryCharStreamReader innerMemoryReader;

	public:
		inline CharsReader(const char* data, uint32 length) : innerMemoryReader(data, length), Base(innerMemoryReader) {}

		inline uint32 getOffset() const { return uint32(innerMemoryReader.getOffset()); }

		inline const char* getData() const { return innerMemoryReader.getData(); }
		inline uint32 getLength() const { return uint32(innerMemoryReader.getLength()); }
	};

	class Lexer : public NonCopyable
	{
	private:
		CharsReader charsReader;

		StringViewASCII currentSourceFilename;
		uint32 currentLexemeBeginOffset = 0;
		uint32 currentLexemeEndOffset = 0;
		uint32 currentLexemeLineNumber = 0;
		uint32 currentLexemeColumnNumber = 0;
		LexemeType currentLexemeType = {};

	private:
		void setCurrentLexeme(LexemeType type, uint32 beginOffset, uint32 endOffset, uint32 lineNumber, uint32 columnNumber);

	public:
		Lexer(StringViewASCII source, StringViewASCII mainSourceFilename);
		~Lexer() = default;

		bool advance(OutputCollector& outputCollector);

		inline LexemeType getCurrentLexemeType() const { return currentLexemeType; }
		inline StringViewASCII getCurrentLexemeString() const;
		inline SourceLocation getCurrentLexemeSourceLocation() const;
		inline uint32 getCurrentLexemeSourceOffset() const { return currentLexemeBeginOffset; }

		inline bool hasLexeme() const { return currentLexemeType != LexemeType(0); }
	};

	class SourcePatcher : public NonCopyable
	{
	private:
		const char* originalSource = nullptr;
		uint32 originalSourceSize = 0;
		uint32 originalSourceOffset = 0;

		DynamicStringASCII result;

	public:
		SourcePatcher(StringViewASCII originalSource);
		~SourcePatcher() = default;

		void advance(uint32 originalSourceOffsetToAdvanceTo, bool forwardOriginalSource);
		void writePatch(StringViewASCII patch);

		DynamicStringASCII finalize();
	};
}


// Lexer ///////////////////////////////////////////////////////////////////////////////////////////

Lexer::Lexer(StringViewASCII source, StringViewASCII mainSourceFilename)
	: charsReader(source.getData(), uint32(source.getLength())), currentSourceFilename(mainSourceFilename) {}

void Lexer::setCurrentLexeme(LexemeType type, uint32 beginOffset, uint32 endOffset, uint32 lineNumber, uint32 columnNumber)
{
	currentLexemeBeginOffset = beginOffset;
	currentLexemeEndOffset = endOffset;
	currentLexemeLineNumber = lineNumber;
	currentLexemeColumnNumber = columnNumber;
	currentLexemeType = type;
}

bool Lexer::advance(OutputCollector& outputCollector)
{
	auto getRealCurrentSourceLocation = [this]() -> SourceLocation
	{
		return SourceLocation
		{
			.filename = currentSourceFilename,
			.lineNumber = charsReader.getLineNumber(),
			.columnNumber = charsReader.getColumnNumber(),
		};
	};

	// Skip whitespaces and comments.

	for (;;)
	{
		FmtSkipWhitespaces(charsReader);

		if (charsReader.peek() != '/')
			break;
		if (charsReader.peek(1) == '/')
		{
			FmtSkipToNewLine(charsReader);
		}
		else if (charsReader.peek(1) == '*')
		{
			charsReader.get();
			charsReader.get();

			for(;;)
			{
				if (charsReader.get() == '*' && charsReader.peek() == '/')
				{
					charsReader.get();
					break;
				}
				if (charsReader.isEndOfStream())
				{
					outputCollector.appendMessageFmt(getRealCurrentSourceLocation(), "lexer: unexpected end-of-file in multiline comment");
					return false;
				}
			}
		}
		else
			break;
	}

	// Process lexeme.

	if (charsReader.isEndOfStream())
	{
		setCurrentLexeme(LexemeType::EndOfFile,
			charsReader.getOffset(), charsReader.getOffset(),
			charsReader.getLineNumber(), charsReader.getColumnNumber());
		return true;
	}

	const uint32 lexemeBeginOffset = charsReader.getOffset();
	const uint32 lexemeBeginLineNumber = charsReader.getLineNumber();
	const uint32 lexemeBeginColumnNumber = charsReader.getColumnNumber();

	const char lexemeFirstChar = charsReader.peek();

	// Identifier
	if (Char::IsLetter(lexemeFirstChar) || lexemeFirstChar == '_')
	{
		charsReader.get();

		while (Char::IsLetterOrDigit(charsReader.peek()) || charsReader.peek() == '_')
			charsReader.get();

		setCurrentLexeme(LexemeType::Identifier,
			lexemeBeginOffset, charsReader.getOffset(),
			lexemeBeginLineNumber, lexemeBeginColumnNumber);
		return true;
	}

	// String literal
	if (lexemeFirstChar == '\"')
	{
		charsReader.get();

		enum class EscapeState : uint8
		{
			Normal = 0,
			BackslashConsumed,
			CRConsumed,
		};
		EscapeState escapeState = EscapeState::Normal;

		for (;;)
		{
			if (charsReader.isEndOfStream())
			{
				outputCollector.appendMessageFmt(getRealCurrentSourceLocation(), "lexer: unexpected end-of-file in string literal");
				return false;
			}

			const char c = charsReader.peek();

			if (c == '\"')
			{
				if (escapeState == EscapeState::BackslashConsumed)
					escapeState = EscapeState::Normal;
				else
					break;
			}
			else if (c == '\\')
			{
				if (escapeState == EscapeState::BackslashConsumed)
					escapeState = EscapeState::Normal;
				else
					escapeState = EscapeState::BackslashConsumed;
			}
			else if (c == '\n')
			{
				if (escapeState == EscapeState::BackslashConsumed || escapeState == EscapeState::CRConsumed)
					escapeState = EscapeState::Normal;
				else
				{
					outputCollector.appendMessageFmt(getRealCurrentSourceLocation(), "lexer: unexpected end-of-line in string literal");
					return false;
				}
			}
			else if (c == '\r')
			{
				if (escapeState == EscapeState::BackslashConsumed)
					escapeState = EscapeState::CRConsumed;
			}
			else
			{
				if (escapeState == EscapeState::BackslashConsumed || escapeState == EscapeState::CRConsumed)
					escapeState = EscapeState::Normal;
			}

			charsReader.get();
		}

		const char closingQuote = charsReader.get();
		XAssert(closingQuote == '\"');

		setCurrentLexeme(LexemeType::StringLiteral,
			lexemeBeginOffset, charsReader.getOffset(),
			lexemeBeginLineNumber, lexemeBeginColumnNumber);
		return true;
	}

	// Numeric literal
	// This is dummy implementation that works as far as we do not need to lex numeric literals properly :)
	if (Char::IsDigit(lexemeFirstChar))
	{
		charsReader.get();

		while (Char::IsLetterOrDigit(charsReader.peek()) || charsReader.peek() == '_')
			charsReader.get();

		setCurrentLexeme(LexemeType::NumericLiteral,
			lexemeBeginOffset, charsReader.getOffset(),
			lexemeBeginLineNumber, lexemeBeginColumnNumber);
		return true;
	}


	if (lexemeFirstChar == ':' && charsReader.peek(1) == ':')
	{
		charsReader.get();
		charsReader.get();

		setCurrentLexeme(LexemeType::DoubleColon,
			lexemeBeginOffset, charsReader.getOffset(),
			lexemeBeginLineNumber, lexemeBeginColumnNumber);
		return true;
	}

	if (lexemeFirstChar > 32 && lexemeFirstChar < 127)
	{
		charsReader.get();

		setCurrentLexeme(LexemeType(lexemeFirstChar),
			lexemeBeginOffset, charsReader.getOffset(),
			lexemeBeginLineNumber, lexemeBeginColumnNumber);
		return true;
	}

	outputCollector.appendMessageFmt(getRealCurrentSourceLocation(), "lexer: invalid character (code=0x", FmtArgHex8(lexemeFirstChar), ")");
	return false;
}

inline StringViewASCII Lexer::getCurrentLexemeString() const
{
	XAssert(currentLexemeBeginOffset <= charsReader.getLength());
	XAssert(currentLexemeEndOffset <= charsReader.getLength());
	XAssert(currentLexemeBeginOffset <= currentLexemeEndOffset);
	return StringViewASCII(charsReader.getData() + currentLexemeBeginOffset, charsReader.getData() + currentLexemeEndOffset);
}

inline SourceLocation Lexer::getCurrentLexemeSourceLocation() const
{
	return SourceLocation
	{
		.filename = currentSourceFilename,
		.lineNumber = currentLexemeLineNumber,
		.columnNumber = currentLexemeColumnNumber,
	};
}


// SourcePatcher ///////////////////////////////////////////////////////////////////////////////////

SourcePatcher::SourcePatcher(StringViewASCII originalSource)
	: originalSource(originalSource.getData()), originalSourceSize(uint32(originalSource.getLength())) {}

void SourcePatcher::advance(uint32 originalSourceOffsetToAdvanceTo, bool forwardOriginalSource)
{
	XAssert(originalSourceOffsetToAdvanceTo <= originalSourceSize);
	XAssert(originalSourceOffset <= originalSourceOffsetToAdvanceTo);

	if (forwardOriginalSource)
	{
		const char* appendRangeStart = originalSource + originalSourceOffset;
		const uintptr appendRangeLength = originalSourceOffsetToAdvanceTo - originalSourceOffset;
		result.append(appendRangeStart, appendRangeLength);
	}

	originalSourceOffset = originalSourceOffsetToAdvanceTo;
}

void SourcePatcher::writePatch(StringViewASCII patch)
{
	result.append(patch);
}

DynamicStringASCII SourcePatcher::finalize()
{
	result.shrinkBuffer();
	return AsRValue(result);
}


////////////////////////////////////////////////////////////////////////////////////////////////////

static bool ProcessAttribute(Lexer& lexer, SourcePatcher& sourcePatcher, Attribute& attribute, OutputCollector& outputCollector)
{
	sourcePatcher.advance(lexer.getCurrentLexemeSourceOffset(), true);

	if (lexer.getCurrentLexemeType() != LexemeType::Identifier)
	{
		// No attribute.
		return true;
	}

	// Should be big enough just to hold known attributes. Overflow is ok for unknown ones.
	InplaceStringASCIIx64 attributeName;
	const SourceLocation attributeNameLocation = lexer.getCurrentLexemeSourceLocation();

	if (lexer.getCurrentLexemeType() != LexemeType::Identifier)
	{
		outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected identifier");
		return false;
	}
	attributeName.append(lexer.getCurrentLexemeString());

	if (!lexer.advance(outputCollector))
		return false;

	// Check for 'namespace::attribute' case.
	if (lexer.getCurrentLexemeType() == LexemeType::DoubleColon)
	{
		if (!lexer.advance(outputCollector))
			return false;
		if (lexer.getCurrentLexemeType() != LexemeType::Identifier)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected identifier");
			return false;
		}
		attributeName.append("::");
		attributeName.append(lexer.getCurrentLexemeString());

		if (!lexer.advance(outputCollector))
			return false;
	}

	if (attributeName == "xe::binding")
	{
		if (attribute.type == AttributeType::Binding)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "attribute list already contains 'xe::binding'");
			return false;
		}

		if (lexer.getCurrentLexemeType() != LexemeType::LeftParen)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected '('");
			return false;
		}

		if (!lexer.advance(outputCollector))
			return false;
		if (lexer.getCurrentLexemeType() != LexemeType::Identifier)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected identifier");
			return false;
		}
		const StringViewASCII bindingRootName = lexer.getCurrentLexemeString();
		const SourceLocation bindingRootNameLocation = lexer.getCurrentLexemeSourceLocation();

		StringViewASCII bindingNestedName = {};
		SourceLocation bindingNestedNameLocation = {};

		if (!lexer.advance(outputCollector))
			return false;
		if (lexer.getCurrentLexemeType() == LexemeType::DoubleColon)
		{
			if (!lexer.advance(outputCollector))
				return false;
			if (lexer.getCurrentLexemeType() != LexemeType::Identifier)
			{
				outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected identifier");
				return false;
			}

			bindingNestedName = lexer.getCurrentLexemeString();
			bindingNestedNameLocation = lexer.getCurrentLexemeSourceLocation();

			if (!lexer.advance(outputCollector))
				return false;
		}
		if (lexer.getCurrentLexemeType() != LexemeType::RightParen)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected ')'");
			return false;
		}

		attribute.binding.rootName = bindingRootName;
		attribute.binding.nestedName = bindingNestedName;
		attribute.binding.rootNameLocation = bindingRootNameLocation;
		attribute.binding.nestedNameLocation = bindingNestedNameLocation;
		attribute.type = AttributeType::Binding;

		if (!lexer.advance(outputCollector))
			return false;
		sourcePatcher.advance(lexer.getCurrentLexemeSourceOffset(), false);
	}
	/*else if (attributeName == "xe::export_cb_layout")
	{

	}*/
	else if (attributeName.startsWith("xe::"))
	{
		outputCollector.appendMessageFmt(attributeNameLocation, "unknown XE attribute '", attributeName, "'");
		return false;
	}
	else
	{
		// Unknown attribute. Just consume everything in parens.

		uint32 openedParenCount = 0;
		if (lexer.getCurrentLexemeType() == LexemeType::LeftParen)
			openedParenCount++;

		while (openedParenCount > 0)
		{
			if (!lexer.advance(outputCollector))
				return false;
			const LexemeType lexemeType = lexer.getCurrentLexemeType();

			if (lexemeType == LexemeType::LeftParen)
				openedParenCount++;
			else if (lexemeType == LexemeType::RightParen)
				openedParenCount--;
			else if (lexemeType == LexemeType::EndOfFile)
			{
				outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "unexpected end-of-file in attribute specifier");
				return false;
			}
		}

		if (!lexer.advance(outputCollector))
			return false;
	}

	return true;
}

static bool ProcessAttributeList(Lexer& lexer, SourcePatcher& sourcePatcher, Attribute& attribute, OutputCollector& outputCollector)
{
	// TODO: Implement `[[attr1]] [[attr2]]` support.

	// Consume attributes separated by commas.
	for (;;)
	{
		if (!ProcessAttribute(lexer, sourcePatcher, attribute, outputCollector))
			return false;

		if (lexer.getCurrentLexemeType() == LexemeType::RightSquareBracket)
		{
			// Consume second ']'.
			if (!lexer.advance(outputCollector))
				return false;
			if (lexer.getCurrentLexemeType() != LexemeType::RightSquareBracket)
			{
				outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected ']'");
				return false;
			}
			if (!lexer.advance(outputCollector))
				return false;
			break;
		}

		if (lexer.getCurrentLexemeType() != LexemeType::Comma)
		{
			if (lexer.getCurrentLexemeType() == LexemeType::EndOfFile)
				outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "unexpected end-of-file in attribute specifier");
			else
				outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "unexpected token in attribute specifier");
			return false;
		}
	}

	return true;
}

static bool ProcessVariableDeclarationForBinding(Lexer& lexer, SourcePatcher& sourcePatcher,
	const BindingInfo& bindingInfo, StringViewASCII displayBindingName, OutputCollector& outputCollector)
{
	if (bindingInfo.type == BindingType::StaticSampler)
	{
		if (lexer.getCurrentLexemeString() != "SamplerState")
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected 'SamplerState'");
			return false;
		}
		if (!lexer.advance(outputCollector))
			return false;

		if (lexer.getCurrentLexemeType() != LexemeType::Identifier)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected identifier");
			return false;
		}
		if (!lexer.advance(outputCollector))
			return false;

		if (lexer.getCurrentLexemeType() != LexemeType::Semicolon)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected ';' ('xe::binding' syntax requirement)");
			return false;
		}
		sourcePatcher.advance(lexer.getCurrentLexemeSourceOffset(), true);

		{
			InplaceStringASCIIx32 registerString;
			FmtPrintStr(registerString, " : register(s", bindingInfo.staticSampler.shaderRegister, ")");
			sourcePatcher.writePatch(registerString);
		}

		// We are currently at a semicolon. Skip to next lexeme.
		if (!lexer.advance(outputCollector))
			return false;
	}
	else if (bindingInfo.type == BindingType::Resource)
	{
		if (lexer.getCurrentLexemeType() != LexemeType::Identifier)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected identifier");
			return false;
		}

		const StringViewASCII resourceTypeStr = lexer.getCurrentLexemeString();
		ResourceType actualResourceType = ResourceType::Undefined;
		if (resourceTypeStr == "ConstantBuffer")
			actualResourceType = ResourceType::ConstantBuffer;
		else if (resourceTypeStr == "Buffer" || resourceTypeStr == "StructuredBuffer")
			actualResourceType = ResourceType::Buffer;
		else if (resourceTypeStr == "RWBuffer")
			actualResourceType = ResourceType::RWBuffer;
		else if (resourceTypeStr == "Texture1D" || resourceTypeStr == "Texture2D" || resourceTypeStr == "Texture3D")
			actualResourceType = ResourceType::Texture;
		else if (resourceTypeStr == "RWTexture1D" || resourceTypeStr == "RWTexture2D" || resourceTypeStr == "RWTexture3D")
			actualResourceType = ResourceType::RWTexture;
		else if (resourceTypeStr == "RaytracingAccelerationStructure")
			actualResourceType = ResourceType::RaytracingAccelerationStructure;
		else
			actualResourceType = ResourceType::Undefined;

		if (actualResourceType != bindingInfo.resource.type)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(),
				"'", resourceTypeStr, "': invalid type to use with pipeline binding '", displayBindingName, "'");
			return false;
		}

		if (!lexer.advance(outputCollector))
			return false;
		if (lexer.getCurrentLexemeType() == LexemeType::LeftAngleBracket)
		{
			if (!lexer.advance(outputCollector))
				return false;
			if (lexer.getCurrentLexemeType() != LexemeType::Identifier)
			{
				outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected identifier");
				return false;
			}

			if (!lexer.advance(outputCollector))
				return false;
			if (lexer.getCurrentLexemeType() != LexemeType::RightAngleBracket)
			{
				outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected '>'. Complex template arguments not supported for now :(");
				return false;
			}
		}

		if (!lexer.advance(outputCollector))
			return false;
		if (lexer.getCurrentLexemeType() != LexemeType::Identifier)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected identifier");
			return false;
		}

		if (!lexer.advance(outputCollector))
			return false;
		if (lexer.getCurrentLexemeType() == LexemeType::LeftSquareBracket)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "arrays are not yet supported :(");
			return false;
		}
		if (lexer.getCurrentLexemeType() != LexemeType::Semicolon)
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "expected ';' ('xe::binding' syntax requirement)");
			return false;
		}
		sourcePatcher.advance(lexer.getCurrentLexemeSourceOffset(), true);

		{
			char shaderRegisterChar = 0;
			const ResourceType type = bindingInfo.resource.type;
			if (type == ResourceType::ConstantBuffer)
				shaderRegisterChar = 'b';
			else if (type == ResourceType::Buffer || type == ResourceType::Texture || type == ResourceType::RaytracingAccelerationStructure)
				shaderRegisterChar = 't';
			else if (type == ResourceType::RWBuffer || type == ResourceType::RWTexture)
				shaderRegisterChar = 'u';
			else
				XAssertUnreachableCode();

			InplaceStringASCIIx32 registerString;
			FmtPrintStr(registerString, " : register(", shaderRegisterChar, registerString, bindingInfo.resource.shaderRegister, ")");
			sourcePatcher.writePatch(registerString);
		}

		// We are currently at a semicolon. Skip to next lexeme.
		if (!lexer.advance(outputCollector))
			return false;
	}
	else
	{
		XAssertUnreachableCode();
		return false;
	}

	return true;
}

static bool ExtractBindingInfo(const PipelineLayout& pipelineLayout,
	StringViewASCII bindingRootName, SourceLocation bindingRootNameLocation,
	StringViewASCII bindingNestedName, SourceLocation bindingNestedNameLocation,
	BindingInfo& resultBindingInfo, OutputCollector& outputCollector)
{
	XAssert(!bindingRootName.isEmpty());

	const sint16 staticSamplerIndex = pipelineLayout.findStaticSampler(bindingRootName);
	if (staticSamplerIndex >= 0)
	{
		if (!bindingNestedName.isEmpty())
		{
			outputCollector.appendMessageFmt(bindingNestedNameLocation,
				"pipeline binding '", bindingRootName, "': nested binding name is not expected");
			return false;
		}

		resultBindingInfo.type = BindingType::StaticSampler;
		resultBindingInfo.staticSampler.shaderRegister = pipelineLayout.getStaticSamplerShaderRegister(staticSamplerIndex);
		return true;
	}

	const sint16 pipelineBindingIndex = pipelineLayout.findBinding(bindingRootName);
	if (pipelineBindingIndex < 0)
	{
		outputCollector.appendMessageFmt(bindingRootNameLocation,
			"unknown pipeline binding '", bindingRootName, "'");
		return false;
	}

	const PipelineBindingDesc pipelineBinding = pipelineLayout.getBindingDesc(pipelineBindingIndex);
	const uint32 pipelineBindingBaseShaderRegiser = pipelineLayout.getBindingBaseShaderRegister(pipelineBindingIndex);

	ResourceType resourceType = ResourceType::Undefined;
	bool allowArray = false;
	uint32 shaderRegister = 0;

	if (pipelineBinding.type == PipelineBindingType::InplaceConstants)
	{
		outputCollector.appendMessageFmt(bindingRootNameLocation,
			"pipeline binding '", bindingRootName, "': inplace constants bindings are not yet supported");
		return false;
	}
	else if (pipelineBinding.type == PipelineBindingType::ConstantBuffer)
	{
		resourceType = ResourceType::ConstantBuffer;
		allowArray = false;
		shaderRegister = pipelineBindingBaseShaderRegiser;
	}
	else if (pipelineBinding.type == PipelineBindingType::ReadOnlyBuffer)
	{
		resourceType = ResourceType::Buffer;
		allowArray = false;
		shaderRegister = pipelineBindingBaseShaderRegiser;
	}
	else if (pipelineBinding.type == PipelineBindingType::ReadWriteBuffer)
	{
		resourceType = ResourceType::RWBuffer;
		allowArray = false;
		shaderRegister = pipelineBindingBaseShaderRegiser;
	}
	else if (pipelineBinding.type == PipelineBindingType::DescriptorSet)
	{
		if (bindingNestedName.isEmpty())
		{
			outputCollector.appendMessageFmt(bindingRootNameLocation,
				"pipeline binding '", bindingRootName, "': descriptor set binding name missing");
			return false;
		}

		XAssert(pipelineBinding.descriptorSetLayout);
		const sint16 descriptorSetBindingIndex =
			pipelineBinding.descriptorSetLayout->findBinding(bindingNestedName);

		if (descriptorSetBindingIndex < 0)
		{
			outputCollector.appendMessageFmt(bindingNestedNameLocation,
				"unknown descriptor set binding '", bindingNestedName, "'");
			return false;
		}

		const DescriptorSetBindingDesc descriptorSetBindingDesc =
			pipelineBinding.descriptorSetLayout->getBindingDesc(descriptorSetBindingIndex);

		switch (descriptorSetBindingDesc.descriptorType)
		{
			case DescriptorType::ReadOnlyBuffer:	resourceType = ResourceType::Buffer;	break;
			case DescriptorType::ReadWriteBuffer:	resourceType = ResourceType::RWBuffer;	break;
			case DescriptorType::ReadOnlyTexture:	resourceType = ResourceType::Texture;	break;
			case DescriptorType::ReadWriteTexture:	resourceType = ResourceType::RWTexture;	break;
			case DescriptorType::RaytracingAccelerationStructure: resourceType = ResourceType::RaytracingAccelerationStructure; break;
			default: XAssertUnreachableCode();
		}

		allowArray = true;
		shaderRegister = pipelineBindingBaseShaderRegiser +
			pipelineBinding.descriptorSetLayout->getBindingDescriptorOffset(descriptorSetBindingIndex);
	}
	else if (pipelineBinding.type == PipelineBindingType::DescriptorArray)
	{
		outputCollector.appendMessageFmt(bindingRootNameLocation,
			"pipeline binding '", bindingRootName, "': descriptor array bindings are not supported for now");
		return false;
	}
	else
		XAssertUnreachableCode();

	if (pipelineBinding.type != PipelineBindingType::DescriptorSet &&
		!bindingNestedName.isEmpty())
	{
		outputCollector.appendMessageFmt(bindingNestedNameLocation,
			"pipeline binding '", bindingRootName, "': nested binding name is not expected");
		return false;
	}

	resultBindingInfo.type = BindingType::Resource;
	resultBindingInfo.resource.shaderRegister = shaderRegister;
	resultBindingInfo.resource.type = resourceType;
	resultBindingInfo.resource.allowArray = allowArray;

	return true;
}

bool ExtPreproc::Preprocess(StringViewASCII source, StringViewASCII mainSourceFilename,
	const PipelineLayout& pipelineLayout, DynamicStringASCII& patchedSource, VirtualStringRefASCII output)
{
	Lexer lexer(source, mainSourceFilename);
	SourcePatcher sourcePatcher(source);
	OutputCollector outputCollector(output);

	if (!lexer.advance(outputCollector))
		return false;

	bool prevLexemeIsLeftSquareBracket = false;
	while (lexer.hasLexeme())
	{
		if (lexer.getCurrentLexemeString() == "register")
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "'register' syntax is banned");
			return false;
		}
		if (lexer.getCurrentLexemeString() == "cbuffer")
		{
			outputCollector.appendMessageFmt(lexer.getCurrentLexemeSourceLocation(), "'cbuffer' syntax is banned");
			return false;
		}

		if (lexer.getCurrentLexemeType() == LexemeType::LeftSquareBracket)
		{
			if (prevLexemeIsLeftSquareBracket)
			{
				if (!lexer.advance(outputCollector))
					return false;

				Attribute attribute = {};
				if (!ProcessAttributeList(lexer, sourcePatcher, attribute, outputCollector))
					return false;

				if (attribute.type == AttributeType::Binding)
				{
					BindingInfo bindingInfo = {};

					if (!ExtractBindingInfo(pipelineLayout,
						attribute.binding.rootName, attribute.binding.rootNameLocation,
						attribute.binding.nestedName, attribute.binding.nestedNameLocation,
						bindingInfo, outputCollector))
					{
						return false;
					}

					InplaceStringASCIIx256 displayBindingName;
					displayBindingName.append(attribute.binding.rootName);
					if (!attribute.binding.nestedName.isEmpty())
					{
						displayBindingName.append("::");
						displayBindingName.append(attribute.binding.nestedName);
					}

					if (!ProcessVariableDeclarationForBinding(lexer, sourcePatcher, bindingInfo, displayBindingName, outputCollector))
						return false;
				}

				prevLexemeIsLeftSquareBracket = false;
				continue;
			}
			else
				prevLexemeIsLeftSquareBracket = true;
		}
		else
			prevLexemeIsLeftSquareBracket = false;

		if (!lexer.advance(outputCollector))
			return false;
	}

	sourcePatcher.advance(lexer.getCurrentLexemeSourceOffset(), true);
	patchedSource = sourcePatcher.finalize();
	return true;
}
