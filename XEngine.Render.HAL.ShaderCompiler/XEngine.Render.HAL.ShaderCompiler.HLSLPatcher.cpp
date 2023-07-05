#include "XEngine.Render.HAL.ShaderCompiler.HLSLPatcher.h"

using namespace XLib;
using namespace XEngine::Render::HAL::ShaderCompiler;

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

void HLSLPatcher::Lexer::advance()
{
	nextLexeme = {};

	TextSkipWhitespaces(textReader);
	if (!textReader.canGetChar())
		return;

	if (Char::IsLetter(textReader.peekCharUnsafe()) || textReader.peekCharUnsafe() == '_')
	{
		const uint32 identifierBeginLineNumber = textReader.getLineNumber();
		const uint32 identifierBeginColumnNumber = textReader.getColumnNumber();
		const char* identifierBegin = textReader.getCurrentPtr();
		textReader.getCharUnsafe();

		while (textReader.canGetChar() && (Char::IsLetterOrDigit(textReader.peekCharUnsafe()) || textReader.peekCharUnsafe() == '_'))
			textReader.getCharUnsafe();

		const char* identifierEnd = textReader.getCurrentPtr();

		nextLexeme.string = StringViewASCII(identifierBegin, identifierEnd);
		nextLexeme.location.lineNumber = identifierBeginLineNumber;
		nextLexeme.location.columnNumber = identifierBeginColumnNumber;
		nextLexeme.type = LexemeType::Identifier;
		return;
	}

#if 0
	if (Char::IsDigit(textReader.peekCharUnsafe()))
	{
		const uint32 numericLiteralBeginLineNumber = textReader.getLineNumber();
		const uint32 numericLiteralBeginColumnNumber = textReader.getColumnNumber();
		const char* numericLiteralBegin = textReader.getCurrentPtr();
		textReader.getCharUnsafe();

		while (textReader.canGetChar())
		{
			const char c = textReader.peekCharUnsafe();

			if (Char::IsLetterOrDigit(c) ||
				c == '_')
		}
	}
#endif

	...
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
		const Lexeme attributeNamePartLexeme = lexer.getLexeme();
		if (attributeNamePartLexeme.type != LexemeType::Identifier)
		{
			error.message = "expected identifier";
			error.location = attributeNamePartLexeme.location;
			return false;
		}

		attributeName.append(attributeNamePartLexeme.string);

		if (lexer.peekLexeme().type != LexemeType::DoubleColon)
			break;
		lexer.getLexeme(); // Consume `::`
		attributeName.append("::");
	}

	if (attributeName == "xe::binding")
	{
		if (attribute.type == AttributeType::Binding)
		{
			// multiple binding attributes
			return false;
		}

		const Lexeme leftParenLexeme = lexer.getLexeme();
		if (leftParenLexeme.type != LexemeType::LeftParen)
		{
			error.message = "expected `(`";
			error.location = leftParenLexeme.location;
			return false;
		}

		const Lexeme bindingRootNameLexeme = lexer.getLexeme();
		if (bindingRootNameLexeme.type != LexemeType::Identifier)
		{
			error.message = "expected identifier";
			error.location = bindingRootNameLexeme.location;
			return false;
		}

		Lexeme bindingNestedNameLexeme = {};
		if (lexer.peekLexeme().type == LexemeType::Dot)
		{
			lexer.getLexeme(); // Consume dot.
			bindingNestedNameLexeme = lexer.getLexeme();
			if (bindingNestedNameLexeme.type != LexemeType::Identifier)
			{
				error.message = "expected identifier";
				error.location = bindingNestedNameLexeme.location;
				return false;
			}
		}

		const Lexeme rightParenLexeme = lexer.getLexeme();
		if (rightParenLexeme.type != LexemeType::RightParen)
		{
			error.message = "expected `)`";
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
			lexer.getLexeme();
			openedParenCount++;
		}

		while (openedParenCount > 0)
		{
			const Lexeme lexeme = lexer.getLexeme();

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

	const Lexeme resourceTypeLexeme = lexer.getLexeme();
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
		lexer.getLexeme();

		const Lexeme bracketedTypeLexeme = lexer.getLexeme();
		if (resourceTypeLexeme.type != LexemeType::Identifier)
		{
			error.message = "expected identifier";
			error.location = bracketedTypeLexeme.location;
			return false;
		}

		const Lexeme closingBracketLexeme = lexer.getLexeme();
		if (closingBracketLexeme.type != LexemeType::RightAngleBracket)
		{
			error.message = "expected `>`. Complex template arguments not supported for now :(";
			error.location = bracketedTypeLexeme.location;
			return false;
		}
	}

	const Lexeme resourceNameLexeme = lexer.getLexeme();
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

	const Lexeme semicolonLexeme = lexer.getLexeme();
	if (semicolonLexeme.type != LexemeType::Semicolon)
	{
		error.message = "expected `;` (`xe::binding` syntax requirement)";
		error.location = semicolonLexeme.location;
		return false;
	}

	// Write `:register(x#);` to output
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
	while (lexer.canGetLexeme())
	{
		const Lexeme lexeme = lexer.getLexeme();

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

		// Check for `[[`.
		if (lexeme.type == LexemeType::LeftSquareBracket &&
			lexer.peekLexeme().type == LexemeType::LeftSquareBracket)
		{
			// Consume second `[`.
			lexer.getLexeme();

			// Consume attributes separated by commas.
			for (;;)
			{
				if (!processAttribute(attribute, error))
					return false;

				const Lexeme nextLexeme = lexer.getLexeme();
				if (nextLexeme.type == LexemeType::RightSquareBracket)
				{
					// Consume second `]`.
					const Lexeme secondRightSquareBracketLexeme = lexer.getLexeme();
					if (secondRightSquareBracketLexeme.type != LexemeType::RightSquareBracket)
					{
						error.message = "expected `]`";
						error.location = secondRightSquareBracketLexeme.location;
						return false;
					}

					break;
				}
				else if (nextLexeme.type != LexemeType::Comma)
				{
					error.message = nextLexeme.type == LexemeType::EndOfStream ?
						"unexpected end of file in attribute specifier" :
						"unexpected token in attribute specifier";
					error.location = nextLexeme.location;
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
