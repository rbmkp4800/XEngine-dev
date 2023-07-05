#include "XEngine.Render.HAL.ShaderCompiler.HLSLPatcher.h"

using namespace XLib;
using namespace XEngine::Render::HAL::ShaderCompiler;

// TODO: Proper numeric literals lexing support.
// TODO: Half of 'HLSLPatcher::Lexer' is copypasted from XJSON. Do something with it.

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
	Semicolon = ':',
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
	//Sampler, ???
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
		TextSkipWhitespaces(textReader);

		if (textReader.getAvailableBytes() < 2)
			break;

		const char* potentialCommentStartPtr = textReader.getCurrentPtr();
		const char c0 = potentialCommentStartPtr[0];
		const char c1 = potentialCommentStartPtr[1];

		const bool isSingleLineComment = c0 == '/' && c1 == '/';
		const bool isMultilineComment = c0 == '/' && c1 == '*';

		if (isSingleLineComment || isMultilineComment)
		{
			textReader.getChar();
			textReader.getChar();
			XAssert(textReader.getCurrentPtr() == potentialCommentStartPtr + 2);
		}

		if (isSingleLineComment)
		{
			for (;;)
			{
				if (!textReader.canGetChar())
					break;
				const char c = textReader.getChar();
				if (c == '\n')
					break;
				if (c == '\\')
				{
					const char c2 = textReader.getChar();
					if (c2 == '\n')
						continue;
					if (c2 == '\r' && textReader.getChar() == '\n')
						continue;
				}
			}
		}
		else if (isMultilineComment)
		{
			// TODO: Use 'TextSkipToFirstOccurrence' instead.

			for (;;)
			{
				if (textReader.getChar() == '*' && textReader.peekChar() == '/')
				{
					textReader.getChar();
					break;
				}
				if (!textReader.canGetChar())
				{
					error.message = "lexer: unexpected end-of-file in multiline comment";
					error.location.lineNumber = textReader.getLineNumber();
					error.location.lineNumber = textReader.getColumnNumber();
					return false;
				}
			}
		}
		else
			break;
	}

	// Process lexeme.

	if (!textReader.canGetChar())
	{
		currentLexeme.string = {};
		currentLexeme.location.lineNumber = textReader.getLineNumber();
		currentLexeme.location.columnNumber = textReader.getColumnNumber();
		currentLexeme.type = LexemeType::EndOfStream;
		return true;
	}

	const uint32 lexemeBeginLineNumber = textReader.getLineNumber();
	const uint32 lexemeBeginColumnNumber = textReader.getColumnNumber();
	const char* lexemeBegin = textReader.getCurrentPtr();

	if (Char::IsLetter(textReader.peekCharUnsafe()) || textReader.peekCharUnsafe() == '_')
	{
		textReader.getCharUnsafe();

		while (textReader.canGetChar() && (Char::IsLetterOrDigit(textReader.peekCharUnsafe()) || textReader.peekCharUnsafe() == '_'))
			textReader.getCharUnsafe();

		currentLexeme.string = StringViewASCII(lexemeBegin, textReader.getCurrentPtr());
		currentLexeme.location.lineNumber = lexemeBeginLineNumber;
		currentLexeme.location.columnNumber = lexemeBeginColumnNumber;
		currentLexeme.type = LexemeType::Identifier;
		return true;
	}

	if (textReader.peekCharUnsafe() == '\"')
	{
		textReader.getCharUnsafe();

		enum class EscapeState : uint8
		{
			Normal = 0,
			BackslashConsumed,
			CRConsumed,
		};
		EscapeState escapeState = EscapeState::Normal;

		for (;;)
		{
			if (!textReader.canGetChar())
			{
				error.message = "lexer: unexpected end-of-file in string literal";
				error.location.lineNumber = textReader.getLineNumber();
				error.location.lineNumber = textReader.getColumnNumber();
				return false;
			}

			const char c = textReader.peekChar();

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
					error.location.lineNumber = textReader.getLineNumber();
					error.location.lineNumber = textReader.getColumnNumber();
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

			textReader.getChar();
		}

		const char closingQuote = textReader.getChar();
		XAssert(closingQuote == '\"');

		currentLexeme.string = StringViewASCII(lexemeBegin, textReader.getCurrentPtr());
		currentLexeme.location.lineNumber = lexemeBeginLineNumber;
		currentLexeme.location.columnNumber = lexemeBeginColumnNumber;
		currentLexeme.type = LexemeType::StringLiteral;
		return true;
	}

	// This is dummy implementation that works as far as we do not need to lex numeric literals properly :)
	if (Char::IsDigit(textReader.peekCharUnsafe()))
	{
		textReader.getCharUnsafe();

		while (textReader.canGetChar() && (Char::IsLetterOrDigit(textReader.peekCharUnsafe()) || textReader.peekCharUnsafe() == '_'))
			textReader.getCharUnsafe();

		currentLexeme.string = StringViewASCII(lexemeBegin, textReader.getCurrentPtr());
		currentLexeme.location.lineNumber = lexemeBeginLineNumber;
		currentLexeme.location.columnNumber = lexemeBeginColumnNumber;
		currentLexeme.type = LexemeType::NumericLiteral;
		return true;
	}

	if (textReader.peekCharUnsafe() > 32 && textReader.peekCharUnsafe() < 127)
	{
		const char c = textReader.getCharUnsafe();

		currentLexeme.string = StringViewASCII(lexemeBegin, textReader.getCurrentPtr());
		currentLexeme.location.lineNumber = lexemeBeginLineNumber;
		currentLexeme.location.columnNumber = lexemeBeginColumnNumber;
		currentLexeme.type = LexemeType(c);
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
	// TODO: Fix location.
	const char* currentRangeEnd = inputLexer.peekLexeme().string.getData();
	output.append(StringViewASCII(currentRangeStart, currentRangeEnd));
	currentRangeStart = currentRangeEnd;
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
		if (lexer.peekLexeme().type == LexemeType::Dot)
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
		TextWriteFmt(error.message, "unknown xe attribute '", attributeName, "'");
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

bool HLSLPatcher::processVariableDefinitionForBinding(const BindingInfo& bindingInfo, Error& error)
{
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
		// TODO: Proper error message
		//TextWriteFmt(error.message, "'", resourceTypeLexeme.string, "': invalid type to use with pipeline binding '", bindingName, "'");
		error.message = "invalid type to use with previously definied binding";
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
		error.message = "arrays not supported for now :(";
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

	// Write ':register(x#);' to output.
	{
		char shaderRegisterChar = 0;
		const ResourceType type = bindingInfo.resource.type;
		if (type == ResourceType::ConstantBuffer)
			shaderRegisterChar = 'b';
		if (type == ResourceType::Buffer || type == ResourceType::Texture || type == ResourceType::RaytracingAccelerationStructure)
			shaderRegisterChar = 't';
		if (type == ResourceType::RWBuffer || type == ResourceType::RWTexture)
			shaderRegisterChar = 'u';
		else
			XAssertUnreachableCode();

		InplaceStringASCIIx32 registerString;
		registerString.append(":register(");
		registerString.append(shaderRegisterChar);
		TextWriteFmt(registerString, bindingInfo.resource.shaderRegister);
		registerString.append(");");
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
	const sint16 pipelineBindingIndex = pipelineLayout.findBinding(bindingRootName);
	if (pipelineBindingIndex < 0)
	{
		TextWriteFmt(error.message, "unknown pipeline binding '", bindingRootName, "'");
		error.location = bindingRootNameLocation;
		return false;
	}

	const PipelineBindingDesc pipelineBinding = pipelineLayout.getBindingDesc(pipelineBindingIndex);
	const uint32 pipelineBindingBaseShaderRegiser = pipelineLayout.getBindingBaseShaderRegister(pipelineBindingIndex);

	ResourceType resourceType = ResourceType::Undefined;
	bool allowArray = false;
	uint32 shaderRegister = 0;

	if (pipelineBinding.type == HAL::PipelineBindingType::InplaceConstants)
	{
		TextWriteFmt(error.message, "pipeline binding '", bindingRootName, "': inplace constants bindings are not supported for now");
		error.location = bindingRootNameLocation;
		return false;
	}
	else if (pipelineBinding.type == HAL::PipelineBindingType::ConstantBuffer)
	{
		resourceType = ResourceType::ConstantBuffer;
		allowArray = false;
		shaderRegister = pipelineBindingBaseShaderRegiser;
	}
	else if (pipelineBinding.type == HAL::PipelineBindingType::ReadOnlyBuffer)
	{
		resourceType = ResourceType::Buffer;
		allowArray = false;
		shaderRegister = pipelineBindingBaseShaderRegiser;
	}
	else if (pipelineBinding.type == HAL::PipelineBindingType::ReadWriteBuffer)
	{
		resourceType = ResourceType::RWBuffer;
		allowArray = false;
		shaderRegister = pipelineBindingBaseShaderRegiser;
	}
	else if (pipelineBinding.type == HAL::PipelineBindingType::DescriptorSet)
	{
		if (bindingNestedName.isEmpty())
		{
			TextWriteFmt(error.message, "pipeline binding '", bindingRootName, "': descriptor set binding name missing");
			error.location = bindingRootNameLocation;
			return false;
		}

		XAssert(pipelineBinding.descriptorSetLayout);
		const sint16 descriptorSetBindingIndex =
			pipelineBinding.descriptorSetLayout->findBinding(bindingNestedName);

		if (descriptorSetBindingIndex < 0)
		{
			TextWriteFmt(error.message, "unknown descriptor set binding '", bindingNestedName, "'");
			error.location = bindingNestedNameLocation;
			return false;
		}

		const DescriptorSetBindingDesc descriptorSetBindingDesc =
			pipelineBinding.descriptorSetLayout->getBindingDesc(descriptorSetBindingIndex);

		ResourceType resourceType = ResourceType::Undefined;
		switch (descriptorSetBindingDesc.descriptorType)
		{
			case HAL::DescriptorType::ReadOnlyBuffer:	resourceType = ResourceType::Buffer;	break;
			case HAL::DescriptorType::ReadWriteBuffer:	resourceType = ResourceType::RWBuffer;	break;
			case HAL::DescriptorType::ReadOnlyTexture:	resourceType = ResourceType::Texture;	break;
			case HAL::DescriptorType::ReadWriteTexture:	resourceType = ResourceType::RWTexture;	break;
			case HAL::DescriptorType::RaytracingAccelerationStructure: resourceType = ResourceType::RaytracingAccelerationStructure; break;
			default: XAssertUnreachableCode();
		}

		resourceType = resourceType;
		allowArray = true;
		shaderRegister = pipelineBindingBaseShaderRegiser +
			pipelineBinding.descriptorSetLayout->getBindingDescriptorOffset(descriptorSetBindingIndex);
	}
	else if (pipelineBinding.type == HAL::PipelineBindingType::DescriptorArray)
	{
		TextWriteFmt(error.message, "pipeline binding '", bindingRootName, "': descriptor array bindings are not supported for now");
		error.location = bindingRootNameLocation;
		return false;
	}
	else
		XAssertUnreachableCode();

	if (pipelineBinding.type != HAL::PipelineBindingType::DescriptorSet &&
		!bindingNestedName.isEmpty())
	{
		TextWriteFmt(error.message, "pipeline binding '", bindingRootName, "': nested binding name is not expected");
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

bool HLSLPatcher::execute(DynamicStringASCII& result, Error& error)
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

			if (!processVariableDefinitionForBinding(bindingInfo, error))
				return false;
		}
	}

	composer.copyInputRangeUpToCurrentPosition();
	result = composer.composeOuput();
	error = {};
	return true;
}
