#include <XLib.Fmt.h>

#include "XEngine.Gfx.HAL.ShaderCompiler.HLSLPatcher.h"

using namespace XLib;
using namespace XEngine::Gfx::HAL::ShaderCompiler;

// TODO: Proper numeric literals lexing support.

enum class HLSLPatcher::LexemeType : uint8
{
	EndOfStream = 0,

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

enum class HLSLPatcher::AttributeType : uint8
{
	None = 0,
	Binding,
	//ExportCBLayout,
};

enum class HLSLPatcher::ResourceType : uint8
{
	Undefined = 0,
	ConstantBuffer,
	Buffer,
	RWBuffer,
	Texture,
	RWTexture,
	RaytracingAccelerationStructure,
};

enum class HLSLPatcher::BindingType : uint8
{
	Undefined = 0,
	Resource,
	Constant,
	StaticSampler,
};

struct HLSLPatcher::Attribute
{
	union
	{
		struct
		{
			StringViewASCII rootName;
			StringViewASCII nestedName;

			Location rootNameLocation;
			Location nestedNameLocation;
		} binding;
	};

	AttributeType type;
};

struct HLSLPatcher::BindingInfo
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

// HLSLPatcher::Lexer //////////////////////////////////////////////////////////////////////////////

bool HLSLPatcher::Lexer::advance(Error& error)
{
	currentLexeme = {};

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
					error.message = "lexer: unexpected end-of-file in multiline comment";
					error.location.lineNumber = charsReader.getLineNumber();
					error.location.columnNumber = charsReader.getColumnNumber();
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
		currentLexeme.string = {};
		currentLexeme.location.lineNumber = charsReader.getLineNumber();
		currentLexeme.location.columnNumber = charsReader.getColumnNumber();
		currentLexeme.type = LexemeType::EndOfStream;
		return true;
	}

	const uint32 lexemeBeginLineNumber = charsReader.getLineNumber();
	const uint32 lexemeBeginColumnNumber = charsReader.getColumnNumber();
	const char* lexemeBeginPtr = charsReader.getCurrentPtr();

	const char lexemeFirstChar = charsReader.peek();

	// Identifier
	if (Char::IsLetter(lexemeFirstChar) || lexemeFirstChar == '_')
	{
		charsReader.get();

		while (Char::IsLetterOrDigit(charsReader.peek()) || charsReader.peek() == '_')
			charsReader.get();

		currentLexeme.string = StringViewASCII(lexemeBeginPtr, charsReader.getCurrentPtr());
		currentLexeme.location.lineNumber = lexemeBeginLineNumber;
		currentLexeme.location.columnNumber = lexemeBeginColumnNumber;
		currentLexeme.type = LexemeType::Identifier;
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
				error.message = "lexer: unexpected end-of-file in string literal";
				error.location.lineNumber = charsReader.getLineNumber();
				error.location.columnNumber = charsReader.getColumnNumber();
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
					error.message = "lexer: unexpected end-of-line in string literal";
					error.location.lineNumber = charsReader.getLineNumber();
					error.location.columnNumber = charsReader.getColumnNumber();
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

		currentLexeme.string = StringViewASCII(lexemeBeginPtr, charsReader.getCurrentPtr());
		currentLexeme.location.lineNumber = lexemeBeginLineNumber;
		currentLexeme.location.columnNumber = lexemeBeginColumnNumber;
		currentLexeme.type = LexemeType::StringLiteral;
		return true;
	}

	// Numeric literal
	// This is dummy implementation that works as far as we do not need to lex numeric literals properly :)
	if (Char::IsDigit(lexemeFirstChar))
	{
		charsReader.get();

		while (Char::IsLetterOrDigit(charsReader.peek()) || charsReader.peek() == '_')
			charsReader.get();

		currentLexeme.string = StringViewASCII(lexemeBeginPtr, charsReader.getCurrentPtr());
		currentLexeme.location.lineNumber = lexemeBeginLineNumber;
		currentLexeme.location.columnNumber = lexemeBeginColumnNumber;
		currentLexeme.type = LexemeType::NumericLiteral;
		return true;
	}


	if (lexemeFirstChar == ':' && charsReader.peek(1) == ':')
	{
		charsReader.get();
		charsReader.get();
		currentLexeme.string = StringViewASCII(lexemeBeginPtr, charsReader.getCurrentPtr());
		currentLexeme.location.lineNumber = lexemeBeginLineNumber;
		currentLexeme.location.columnNumber = lexemeBeginColumnNumber;
		currentLexeme.type = LexemeType::DoubleColon;
		return true;
	}

	if (lexemeFirstChar > 32 && lexemeFirstChar < 127)
	{
		charsReader.get();

		currentLexeme.string = StringViewASCII(lexemeBeginPtr, charsReader.getCurrentPtr());
		currentLexeme.location.lineNumber = lexemeBeginLineNumber;
		currentLexeme.location.columnNumber = lexemeBeginColumnNumber;
		currentLexeme.type = LexemeType(lexemeFirstChar);
		return true;
	}

	error.message = "lexer: invalid character";
	error.location.lineNumber = lexemeBeginLineNumber;
	error.location.lineNumber = lexemeBeginColumnNumber;
	return false;
}

// HLSLPatcher::OutputComposer /////////////////////////////////////////////////////////////////////

void HLSLPatcher::OutputComposer::write(const StringViewASCII& text)
{
	output.append(text);
}

void HLSLPatcher::OutputComposer::copyInputRangeUpToCurrentPosition()
{
	const char* currentRangeEnd = inputLexer.hasLexeme() ? inputLexer.peekLexeme().string.getData() : inputLexer.getEndPtr();
	XAssert(currentRangeEnd && currentRangeStart <= currentRangeEnd);
	output.append(StringViewASCII(currentRangeStart, currentRangeEnd));
	currentRangeStart = currentRangeEnd;

	// TODO: Fix location.
}

void HLSLPatcher::OutputComposer::blankOutInputRangeUpToCurrentPosition()
{
	// TODO: Fix location.
	const char* currentRangeEnd = inputLexer.peekLexeme().string.getData();
	currentRangeStart = currentRangeEnd;
}

// HLSLPatcher /////////////////////////////////////////////////////////////////////////////////////

bool HLSLPatcher::processAttribute(Attribute& attribute, Error& error)
{
	composer.copyInputRangeUpToCurrentPosition();

	if (lexer.peekLexeme().type != LexemeType::Identifier)
	{
		// No attribute.
		return true;
	}

	// Should be big enough just to hold known attributes. Overflow is ok for unknown ones.
	InplaceStringASCIIx64 attributeName;
	const Location attributeNameLocation = lexer.peekLexeme().location;

	for (;;)
	{
		const Lexeme attributeNamePartLexeme = lexer.peekLexeme();
		if (!lexer.advance(error))
			return false;

		if (attributeNamePartLexeme.type != LexemeType::Identifier)
		{
			error.message = "expected identifier";
			error.location = attributeNamePartLexeme.location;
			return false;
		}

		attributeName.append(attributeNamePartLexeme.string);

		// Consume '::'
		if (lexer.peekLexeme().type != LexemeType::DoubleColon)
			break;
		if (!lexer.advance(error))
			return false;
		attributeName.append("::");
	}

	if (attributeName == "xe::binding")
	{
		if (attribute.type == AttributeType::Binding)
		{
			// multiple binding attributes
			return false;
		}

		const Lexeme leftParenLexeme = lexer.peekLexeme();
		if (!lexer.advance(error))
			return false;
		if (leftParenLexeme.type != LexemeType::LeftParen)
		{
			error.message = "expected '('";
			error.location = leftParenLexeme.location;
			return false;
		}

		const Lexeme bindingRootNameLexeme = lexer.peekLexeme();
		if (!lexer.advance(error))
			return false;
		if (bindingRootNameLexeme.type != LexemeType::Identifier)
		{
			error.message = "expected identifier";
			error.location = bindingRootNameLexeme.location;
			return false;
		}

		Lexeme bindingNestedNameLexeme = {};
		if (lexer.peekLexeme().type == LexemeType::DoubleColon)
		{
			if (!lexer.advance(error))
				return false;

			bindingNestedNameLexeme = lexer.peekLexeme();
			if (!lexer.advance(error))
				return false;

			if (bindingNestedNameLexeme.type != LexemeType::Identifier)
			{
				error.message = "expected identifier";
				error.location = bindingNestedNameLexeme.location;
				return false;
			}
		}

		const Lexeme rightParenLexeme = lexer.peekLexeme();
		if (!lexer.advance(error))
			return false;

		if (rightParenLexeme.type != LexemeType::RightParen)
		{
			error.message = "expected ')'";
			error.location = rightParenLexeme.location;
			return false;
		}

		attribute.binding.rootName = bindingRootNameLexeme.string;
		attribute.binding.nestedName = bindingNestedNameLexeme.string;
		attribute.binding.rootNameLocation = bindingRootNameLexeme.location;
		attribute.binding.nestedNameLocation = bindingNestedNameLexeme.location;
		attribute.type = AttributeType::Binding;

		composer.blankOutInputRangeUpToCurrentPosition();
	}
	/*else if (attributeName == "xe::export_cb_layout")
	{

	}*/
	else if (attributeName.startsWith("xe::"))
	{
		FmtPrintStr(error.message, "unknown XE attribute '", attributeName, "'");
		error.location = attributeNameLocation;
		return false;
	}
	else
	{
		// Unknown attribute. Just consume everything in parens.

		uint32 openedParenCount = 0;

		if (lexer.peekLexeme().type == LexemeType::LeftParen)
		{
			if (!lexer.advance(error))
				return false;
			openedParenCount++;
		}

		while (openedParenCount > 0)
		{
			const Lexeme lexeme = lexer.peekLexeme();
			if (!lexer.advance(error))
				return false;

			if (lexeme.type == LexemeType::LeftParen)
				openedParenCount++;
			else if (lexeme.type == LexemeType::RightParen)
				openedParenCount--;
			else if (lexeme.type == LexemeType::EndOfStream)
			{
				error.message = "unexpected end of file";
				error.location = lexeme.location;
				return false;
			}
		}
	}

	return true;
}

bool HLSLPatcher::processVariableDefinitionForBinding(const BindingInfo& bindingInfo, StringViewASCII displayBindingName, Error& error)
{
	error.message.clear();

	if (bindingInfo.type == BindingType::StaticSampler)
	{
		const Lexeme samplerStateLexeme = lexer.peekLexeme();
		if (!lexer.advance(error))
			return false;

		if (samplerStateLexeme.string != "SamplerState")
		{
			error.message = "expected 'SamplerState'";
			error.location = samplerStateLexeme.location;
			return false;
		}

		const Lexeme samplerVarNameLexeme = lexer.peekLexeme();
		if (!lexer.advance(error))
			return false;

		if (samplerVarNameLexeme.type != LexemeType::Identifier)
		{
			error.message = "expected identifier";
			error.location = samplerVarNameLexeme.location;
			return false;
		}

		composer.copyInputRangeUpToCurrentPosition();

		const Lexeme semicolonLexeme = lexer.peekLexeme();
		if (!lexer.advance(error))
			return false;

		if (semicolonLexeme.type != LexemeType::Semicolon)
		{
			error.message = "expected ';' ('xe::binding' syntax requirement)";
			error.location = semicolonLexeme.location;
			return false;
		}

		// Write " : register(s#);" to output.
		InplaceStringASCIIx32 registerString;
		FmtPrintStr(registerString, " : register(s", bindingInfo.staticSampler.shaderRegister, ");\n");
		composer.write(registerString);
		composer.blankOutInputRangeUpToCurrentPosition();

		return true;
	}

	XAssert(bindingInfo.type == BindingType::Resource);

	const Lexeme resourceTypeLexeme = lexer.peekLexeme();
	if (!lexer.advance(error))
		return false;

	if (resourceTypeLexeme.type != LexemeType::Identifier)
	{
		error.message = "expected identifier";
		error.location = resourceTypeLexeme.location;
		return false;
	}

	ResourceType actualResourceType = ResourceType::Undefined;
	if (resourceTypeLexeme.string == "ConstantBuffer")
		actualResourceType = ResourceType::ConstantBuffer;
	else if (resourceTypeLexeme.string == "Buffer")
		actualResourceType = ResourceType::Buffer;
	else if (resourceTypeLexeme.string == "RWBuffer")
		actualResourceType = ResourceType::RWBuffer;
	else if (resourceTypeLexeme.string == "Texture1D" || resourceTypeLexeme.string == "Texture2D" || resourceTypeLexeme.string == "Texture3D")
		actualResourceType = ResourceType::Texture;
	else if (resourceTypeLexeme.string == "RWTexture1D" || resourceTypeLexeme.string == "RWTexture2D" || resourceTypeLexeme.string == "RWTexture3D")
		actualResourceType = ResourceType::RWTexture;
	else if (resourceTypeLexeme.string == "RaytracingAccelerationStructure")
		actualResourceType = ResourceType::RaytracingAccelerationStructure;
	else
		actualResourceType = ResourceType::Undefined;

	if (actualResourceType != bindingInfo.resource.type)
	{
		FmtPrintStr(error.message, "'", resourceTypeLexeme.string, "': invalid type to use with pipeline binding '", displayBindingName, "'");
		error.location = resourceTypeLexeme.location;
		return false;
	}

	if (lexer.peekLexeme().type == LexemeType::LeftAngleBracket)
	{
		if (!lexer.advance(error))
			return false;

		const Lexeme bracketedTypeLexeme = lexer.peekLexeme();
		if (!lexer.advance(error))
			return false;

		if (resourceTypeLexeme.type != LexemeType::Identifier)
		{
			error.message = "expected identifier";
			error.location = bracketedTypeLexeme.location;
			return false;
		}

		const Lexeme closingBracketLexeme = lexer.peekLexeme();
		if (!lexer.advance(error))
			return false;

		if (closingBracketLexeme.type != LexemeType::RightAngleBracket)
		{
			error.message = "expected '>'. Complex template arguments not supported for now :(";
			error.location = bracketedTypeLexeme.location;
			return false;
		}
	}

	const Lexeme resourceNameLexeme = lexer.peekLexeme();
	if (!lexer.advance(error))
		return false;

	if (resourceNameLexeme.type != LexemeType::Identifier)
	{
		error.message = "expected identifier";
		error.location = resourceNameLexeme.location;
		return false;
	}

	if (lexer.peekLexeme().type == LexemeType::LeftSquareBracket)
	{
		error.message = "arrays are not yet supported :(";
		error.location = lexer.peekLexeme().location;
		XAssertUnreachableCode();
		return false;
	}

	composer.copyInputRangeUpToCurrentPosition();

	const Lexeme semicolonLexeme = lexer.peekLexeme();
	if (!lexer.advance(error))
		return false;

	if (semicolonLexeme.type != LexemeType::Semicolon)
	{
		error.message = "expected ';' ('xe::binding' syntax requirement)";
		error.location = semicolonLexeme.location;
		return false;
	}

	// Write " : register(x#);" to output.
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
		FmtPrintStr(registerString, " : register(", shaderRegisterChar, registerString, bindingInfo.resource.shaderRegister, ");\n");
		composer.write(registerString);
	}

	composer.blankOutInputRangeUpToCurrentPosition();

	return true;
}

bool HLSLPatcher::ExtractBindingInfo(const PipelineLayout& pipelineLayout,
	StringViewASCII bindingRootName, Location bindingRootNameLocation,
	StringViewASCII bindingNestedName, Location bindingNestedNameLocation,
	BindingInfo& resultBindingInfo, Error& error)
{
	const sint16 staticSamplerIndex = pipelineLayout.findStaticSampler(bindingRootName);
	if (staticSamplerIndex >= 0)
	{
		if (!bindingNestedName.isEmpty())
		{
			FmtPrintStr(error.message, "pipeline binding '", bindingRootName, "': nested binding name is not expected");
			error.location = bindingNestedNameLocation;
			return false;
		}

		resultBindingInfo.type = BindingType::StaticSampler;
		resultBindingInfo.staticSampler.shaderRegister = pipelineLayout.getStaticSamplerShaderRegister(staticSamplerIndex);
		return true;
	}

	const sint16 pipelineBindingIndex = pipelineLayout.findBinding(bindingRootName);
	if (pipelineBindingIndex < 0)
	{
		FmtPrintStr(error.message, "unknown pipeline binding '", bindingRootName, "'");
		error.location = bindingRootNameLocation;
		return false;
	}

	const PipelineBindingDesc pipelineBinding = pipelineLayout.getBindingDesc(pipelineBindingIndex);
	const uint32 pipelineBindingBaseShaderRegiser = pipelineLayout.getBindingBaseShaderRegister(pipelineBindingIndex);

	ResourceType resourceType = ResourceType::Undefined;
	bool allowArray = false;
	uint32 shaderRegister = 0;

	if (pipelineBinding.type == PipelineBindingType::InplaceConstants)
	{
		FmtPrintStr(error.message, "pipeline binding '", bindingRootName, "': inplace constants bindings are not yet supported");
		error.location = bindingRootNameLocation;
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
			FmtPrintStr(error.message, "pipeline binding '", bindingRootName, "': descriptor set binding name missing");
			error.location = bindingRootNameLocation;
			return false;
		}

		XAssert(pipelineBinding.descriptorSetLayout);
		const sint16 descriptorSetBindingIndex =
			pipelineBinding.descriptorSetLayout->findBinding(bindingNestedName);

		if (descriptorSetBindingIndex < 0)
		{
			FmtPrintStr(error.message, "unknown descriptor set binding '", bindingNestedName, "'");
			error.location = bindingNestedNameLocation;
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
		FmtPrintStr(error.message, "pipeline binding '", bindingRootName, "': descriptor array bindings are not supported for now");
		error.location = bindingRootNameLocation;
		return false;
	}
	else
		XAssertUnreachableCode();

	if (pipelineBinding.type != PipelineBindingType::DescriptorSet &&
		!bindingNestedName.isEmpty())
	{
		FmtPrintStr(error.message, "pipeline binding '", bindingRootName, "': nested binding name is not expected");
		error.location = bindingNestedNameLocation;
		return false;
	}

	resultBindingInfo.type = BindingType::Resource;
	resultBindingInfo.resource.shaderRegister = shaderRegister;
	resultBindingInfo.resource.type = resourceType;
	resultBindingInfo.resource.allowArray = allowArray;

	return true;
}

HLSLPatcher::HLSLPatcher(StringViewASCII sourceText, const PipelineLayout& pipelineLayout)
	: lexer(sourceText), composer(lexer), pipelineLayout(pipelineLayout) { }

bool HLSLPatcher::patch(DynamicStringASCII& result, Error& error)
{
	if (!lexer.advance(error))
		return false;

	while (lexer.hasLexeme())
	{
		Lexeme lexeme = lexer.peekLexeme();
		if (!lexer.advance(error))
			return false;

		if (lexeme.string == "register")
		{
			error.message = "'register' syntax is banned";
			error.location = lexeme.location;
			return false;
		}
		if (lexeme.string == "cbuffer")
		{
			error.message = "'cbuffer' syntax is banned";
			error.location = lexeme.location;
			return false;
		}

		Attribute attribute = {};

		// Check for '[['.
		if (lexeme.type == LexemeType::LeftSquareBracket &&
			lexer.peekLexeme().type == LexemeType::LeftSquareBracket)
		{
			// Consume second '['.
			if (!lexer.advance(error))
				return false;

			// Consume attributes separated by commas.
			for (;;)
			{
				if (!processAttribute(attribute, error))
					return false;

				lexeme = lexer.peekLexeme();
				if (!lexer.advance(error))
					return false;

				if (lexeme.type == LexemeType::RightSquareBracket)
				{
					// Consume second ']'.
					const Lexeme secondRightSquareBracketLexeme = lexer.peekLexeme();
					if (!lexer.advance(error))
						return false;

					if (secondRightSquareBracketLexeme.type != LexemeType::RightSquareBracket)
					{
						error.message = "expected ']'";
						error.location = secondRightSquareBracketLexeme.location;
						return false;
					}

					break;
				}
				else if (lexeme.type != LexemeType::Comma)
				{
					error.message = lexeme.type == LexemeType::EndOfStream ?
						"unexpected end of file in attribute specifier" :
						"unexpected token in attribute specifier";
					error.location = lexeme.location;
					return false;
				}
			}
		}

		if (attribute.type == AttributeType::Binding)
		{
			BindingInfo bindingInfo = {};

			if (!ExtractBindingInfo(pipelineLayout,
				attribute.binding.rootName, attribute.binding.rootNameLocation,
				attribute.binding.nestedName, attribute.binding.nestedNameLocation,
				bindingInfo, error))
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

			if (!processVariableDefinitionForBinding(bindingInfo, displayBindingName, error))
				return false;
		}
	}

	composer.copyInputRangeUpToCurrentPosition();
	result = composer.composeOuput();
	error = {};
	return true;
}

bool HLSLPatcher::Patch(StringViewASCII sourceText, const PipelineLayout& pipelineLayout,
	DynamicStringASCII& result, Error& error)
{
	HLSLPatcher patcher(sourceText, pipelineLayout);
	return patcher.patch(result, error);
}
