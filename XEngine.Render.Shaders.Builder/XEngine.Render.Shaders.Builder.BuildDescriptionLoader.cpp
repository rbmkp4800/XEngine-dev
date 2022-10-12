#include <XLib.Containers.ArrayList.h>
#include <XLib.String.h>
#include <XLib.Text.h>

#include <XEngine.Render.HAL.Common.h>
#include <XEngine.Render.HAL.ShaderCompiler.h>

#include "XEngine.Render.Shaders.Builder.BuildDescriptionLoader.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder_;

auto BuildDescriptionLoader::ParseTexelViewFormatString(StringViewASCII string) -> HAL::TexelViewFormat
{
	// TODO: Do this properly.
	if (String::IsEqual(string, "R8G8B8BA8_Unorm"))
		return HAL::TexelViewFormat::R8G8B8A8_UNORM;

	return HAL::TexelViewFormat(-1);
}

auto BuildDescriptionLoader::ParseDepthStencilFormatString(StringViewASCII string) -> HAL::DepthStencilFormat
{
	// TODO: Do this properly.
	if (String::IsEqual(string, "D32"))
		return HAL::DepthStencilFormat::D32;

	return HAL::DepthStencilFormat(-1);
}

bool BuildDescriptionLoader::expectSimpleToken(TokenType type)
{
	const Token token = tokenizer.getToken();
	if (token.type != type)
	{
		reportError("expected token ...", token); // TODO: Put expected token here.
		return false;
	}
	return true;
}

bool BuildDescriptionLoader::parseDescriptorSetLayoutDeclaration()
{
	const Token descriptorSetLayoutNameToken = tokenizer.getToken();
	if (descriptorSetLayoutNameToken.type != TokenType::Identifier)
	{
		reportError("expected descriptor set layout name", descriptorSetLayoutNameToken);
		return false;
	}

	if (!expectSimpleToken(TokenType('{')))
		return false;

	InplaceArrayList<HAL::ShaderCompiler::DescriptorSetBindingDesc, HAL::MaxDescriptorSetBindingCount> bindings;

	for (;;)
	{
		const Token token = tokenizer.getToken();
		if (token.type == TokenType('}'))
			break;
		if (token.type == PseudoCTokenizer::TokenType::Semicolon)
			continue;

		if (token.type != TokenType::Identifier)
		{
			reportError("expected descriptor type", token);
			return false;
		}

		HAL::ShaderCompiler::DescriptorSetBindingDesc binding = {};

		if (String::IsEqual(token.string, "ReadOnlyBufferDescriptor"))
			binding.descriptorType = HAL::DescriptorType::ReadOnlyBuffer;
		else if (String::IsEqual(token.string, "ReadWriteBufferDescriptor"))
			binding.descriptorType = HAL::DescriptorType::ReadWriteBuffer;
		else if (String::IsEqual(token.string, "ReadOnlyTextureDescriptor"))
			binding.descriptorType = HAL::DescriptorType::ReadOnlyTexture;
		else if (String::IsEqual(token.string, "ReadWriteTextureDescriptor"))
			binding.descriptorType = HAL::DescriptorType::ReadWriteTexture;
		else if (String::IsEqual(token.string, "RaytracingAccelerationStructureDescriptor"))
			binding.descriptorType = HAL::DescriptorType::RaytracingAccelerationStructure;
		else
		{
			reportError("unknown descriptor type", token);
			return false;
		}

		const Token descriptorNameToken = tokenizer.getToken();
		if (descriptorNameToken.type != TokenType::Identifier)
		{
			reportError("expected descriptor name", descriptorNameToken);
			return false;
		}

		binding.name = descriptorNameToken.string;

		if (!expectSimpleToken(TokenType(';')))
			return false;

		if (bindings.isFull())
		{
			reportError("too many descriptors", token);
			return false;
		}

		bindings.pushBack(binding);
	}

	const DescriptorSetLayoutCreationResult descriptorSetLayoutCreationResult =
		descriptorSetLayoutList.create(descriptorSetLayoutNameToken.string, bindings, uint8(bindings.getSize()));

	if (descriptorSetLayoutCreationResult.status != DescriptorSetLayoutCreationStatus::Success)
	{
		// TODO: Proper error handling (hash collision etc).
		reportError("descriptor set layout redefinition", descriptorSetLayoutNameToken);
		return false;
	}

	return true;
}

bool BuildDescriptionLoader::parsePipelineLayoutDeclaration()
{
	const Token pipelineLayoutNameToken = tokenizer.getToken();
	if (pipelineLayoutNameToken.type != TokenType::Identifier)
	{
		reportError("expected pipeline layout name", pipelineLayoutNameToken);
		return false;
	}

	if (!expectSimpleToken(TokenType('{')))
		return false;

	InplaceArrayList<PipelineBindingDesc, HAL::MaxPipelineBindingCount> bindings;

	for (;;)
	{
		const Token token = tokenizer.getToken();
		if (token.type == TokenType('}'))
			break;
		if (token.type == PseudoCTokenizer::TokenType::Semicolon)
			continue;

		if (token.type != TokenType::Identifier)
		{
			reportError("expected binding type", token);
			return false;
		}

		PipelineBindingDesc binding = {};

		if (String::IsEqual(token.string, "ConstantBuffer"))
			binding.type = HAL::PipelineBindingType::ConstantBuffer;
		else if (String::IsEqual(token.string, "ReadOnlyBuffer"))
			binding.type = HAL::PipelineBindingType::ReadOnlyBuffer;
		else if (String::IsEqual(token.string, "ReadWriteBuffer"))
			binding.type = HAL::PipelineBindingType::ReadWriteBuffer;
		else if (String::IsEqual(token.string, "DescriptorSet"))
		{
			if (!expectSimpleToken(TokenType('<')))
				return false;

			const Token layoutNameToken = tokenizer.getToken();
			if (layoutNameToken.type != TokenType::Identifier)
			{
				reportError("expected descriptor set layout name", layoutNameToken);
				return false;
			}

			if (!expectSimpleToken(TokenType('>')))
				return false;

			DescriptorSetLayout* descriptorSetLayout = descriptorSetLayoutList.find(layoutNameToken.string);
			if (!descriptorSetLayout)
			{
				reportError("undefined descriptor set layout", layoutNameToken);
				return false;
			}

			binding.type = HAL::PipelineBindingType::DescriptorSet;
			binding.descriptorSetLayout = descriptorSetLayout;
		}
		else
		{
			reportError("unknown binding type", token);
			return false;
		}

		const Token bindingNameToken = tokenizer.getToken();
		if (bindingNameToken.type != TokenType::Identifier)
		{
			reportError("expected binding name", bindingNameToken);
			return false;
		}

		binding.name = bindingNameToken.string;

		if (!expectSimpleToken(TokenType(';')))
			return false;

		if (bindings.isFull())
		{
			reportError("too many bindings", token);
			return false;
		}

		bindings.pushBack(binding);
	}

	const PipelineLayoutCreationResult pipelineLayoutCreationResult =
		pipelineLayoutList.create(pipelineLayoutNameToken.string, bindings, uint8(bindings.getSize()));
	
	if (pipelineLayoutCreationResult.status != PipelineLayoutCreationStatus::Success)
	{
		// TODO: Proper error handling (hash collision etc).
		reportError("pipeline layout redefinition", pipelineLayoutNameToken);
		return false;
	}

	return true;
}

bool BuildDescriptionLoader::parseGraphicsPipelineDeclaration()
{
	const Token pipelineNameToken = tokenizer.getToken();
	if (pipelineNameToken.type != TokenType::Identifier)
	{
		reportError("expected pipeline name", pipelineNameToken);
		return false;
	}

	if (!expectSimpleToken(TokenType('{')))
		return false;

	PipelineLayout* pipelineLayout = nullptr;
	GraphicsPipelineDesc pipelineDesc = {};
	uint8 setRTsMask = 0;
	bool depthRTIsSet = false;

	for (;;)
	{
		const Token token = tokenizer.getToken();
		if (token.type == TokenType('}'))
			break;
		if (token.type == TokenType::Semicolon)
			continue;

		if (token.type != TokenType::Identifier)
		{
			reportError("expected graphics pipeline setup statement", token);
			return false;
		}

		if (String::IsEqual(token.string, "SetLayout"))
		{
			if (pipelineLayout)
			{
				reportError("pipeline layout already defined", token);
				return false;
			}

			pipelineLayout = parseSetLayoutStatement();
			if (!pipelineLayout)
				return false;
		}
		else if (String::IsEqual(token.string, "SetVS"))
		{
			if (pipelineDesc.vertexShader)
			{
				reportError("vertex shader already defined", token);
				return false;
			}
			if (!pipelineLayout)
			{
				reportError("pipeline layout should be defined prior to shaders", token);
				return false;
			}

			pipelineDesc.vertexShader = parseSetShaderStatement(HAL::ShaderCompiler::ShaderType::Vertex, *pipelineLayout);
			if (!pipelineDesc.vertexShader)
				return false;
		}
		else if (String::IsEqual(token.string, "SetPS"))
		{
			if (pipelineDesc.pixelShader)
			{
				reportError("pixel shader already defined", token);
				return false;
			}
			if (!pipelineLayout)
			{
				reportError("pipeline layout should be defined prior to shaders", token);
				return false;
			}

			pipelineDesc.pixelShader = parseSetShaderStatement(HAL::ShaderCompiler::ShaderType::Pixel, *pipelineLayout);
			if (!pipelineDesc.pixelShader)
				return false;
		}
		else if (String::IsEqual(token.string, "SetRT"))
		{
			if (!expectSimpleToken(TokenType('(')))
				return false;

			const Token rtIndexToken = tokenizer.getToken();
			if (rtIndexToken.type != TokenType::NumberLiteral)
			{
				reportError("expected render target index", rtIndexToken);
				return false;
			}
			if (rtIndexToken.number < 0 || rtIndexToken.number >= HAL::MaxRenderTargetCount)
			{
				reportError("invalid render target index", rtIndexToken);
				return false;
			}

			const uint8 rtIndex = uint8(rtIndexToken.number);

			const bool rtIsAlreadySet = (setRTsMask & (1 << rtIndex)) != 0;
			if (rtIsAlreadySet)
			{
				reportError("this render target is already set", rtIndexToken);
				return false;
			}

			if (!expectSimpleToken(TokenType(',')))
				return false;

			const Token rtFormatToken = tokenizer.getToken();
			if (rtFormatToken.type != TokenType::Identifier)
			{
				reportError("expected render target format identifier", rtFormatToken);
				return false;
			}

			const HAL::TexelViewFormat rtFormat = ParseTexelViewFormatString(rtFormatToken.string);
			if (!HAL::ValidateTexelViewFormatValue(rtFormat))
			{
				reportError("unknown format", rtFormatToken);
				return false;
			}

			if (!HAL::ValidateTexelViewFormatForRenderTargetUsage(rtFormat))
			{
				reportError("this format is not supported for render target usage", rtFormatToken);
				return false;
			}

			if (!expectSimpleToken(TokenType(')')))
				return false;

			pipelineDesc.renderTargetsFormats[rtIndex] = rtFormat;
			setRTsMask |= 1 << rtIndex;
		}
		else if (String::IsEqual(token.string, "SetDepthRT"))
		{
			if (depthRTIsSet)
			{
				reportError("depth render target is already set", token);
				return false;
			}

			if (!expectSimpleToken(TokenType('(')))
				return false;

			const Token depthRTFormatToken = tokenizer.getToken();
			if (depthRTFormatToken.type != TokenType::Identifier)
			{
				reportError("expected depth render target format identifier", depthRTFormatToken);
				return false;
			}

			const HAL::DepthStencilFormat depthRTFormat = ParseDepthStencilFormatString(depthRTFormatToken.string);
			//if (!HAL::ValidateDepthStencilFormatValue(depthRTFormat))
			//{
			//	reportError("unknown depth render target format");
			//	return false;
			//}

			if (!expectSimpleToken(TokenType(')')))
				return false;

			pipelineDesc.depthStencilFormat = depthRTFormat;
			depthRTIsSet = true;
		}
		else
		{
			reportError("unknown graphics pipeline setup statement", token);
			return false;
		}

		if (!expectSimpleToken(TokenType(';')))
			return false;
	}

	if (!pipelineLayout)
	{
		reportError("pipeline layout is not defined", pipelineNameToken);
		return false;
	}

	// TODO: More validation (shader combinations etc).

	const PipelineCreationResult pipelineCreationResult =
		pipelineList.create(pipelineNameToken.string, *pipelineLayout, &pipelineDesc, nullptr);
	if (pipelineCreationResult.status != PipelineCreationStatus::Success)
	{
		// TODO: Proper error handling (hash collision etc).
		reportError("pipeline with this name already defined", pipelineNameToken);
		return false;
	}

	return true;
}

bool BuildDescriptionLoader::parseComputePipelineDeclaration()
{
	const Token pipelineNameToken = tokenizer.getToken();
	if (pipelineNameToken.type != TokenType::Identifier)
	{
		reportError("expected pipeline name", pipelineNameToken);
		return false;
	}

	if (!expectSimpleToken(TokenType('{')))
		return false;

	PipelineLayout* pipelineLayout = nullptr;
	Shader* computeShader = nullptr;

	for (;;)
	{
		const Token token = tokenizer.getToken();
		if (token.type == TokenType('}'))
			break;
		if (token.type == TokenType::Semicolon)
			continue;

		if (token.type != TokenType::Identifier)
		{
			reportError("expected compute pipeline setup statement", token);
			return false;
		}

		if (String::IsEqual(token.string, "SetLayout"))
		{
			if (pipelineLayout)
			{
				reportError("pipeline layout already defined", token);
				return false;
			}

			pipelineLayout = parseSetLayoutStatement();
			if (!pipelineLayout)
				return false;
		}
		else if (String::IsEqual(token.string, "SetCS"))
		{
			if (computeShader)
			{
				reportError("compute shader already defined", token);
				return false;
			}
			if (!pipelineLayout)
			{
				reportError("pipeline layout should be defined prior to shaders", token);
				return false;
			}

			computeShader = parseSetShaderStatement(HAL::ShaderCompiler::ShaderType::Compute, *pipelineLayout);
			if (!computeShader)
				return false;
		}
		else
		{
			reportError("unknown compute pipeline setup statement", token);
			return false;
		}

		if (!expectSimpleToken(TokenType(';')))
			return false;
	}

	if (!pipelineLayout)
	{
		reportError("pipeline layout is not defined", pipelineNameToken);
		return false;
	}

	if (!computeShader)
	{
		reportError("compute shader is not defined", pipelineNameToken);
		return false;
	}

	const PipelineCreationResult pipelineCreationResult =
		pipelineList.create(pipelineNameToken.string, *pipelineLayout, nullptr, computeShader);
	if (pipelineCreationResult.status != PipelineCreationStatus::Success)
	{
		// TODO: Proper error handling (hash collision etc).
		reportError("pipeline with this name already defined", pipelineNameToken);
		return false;
	}

	return true;
}

PipelineLayout* BuildDescriptionLoader::parseSetLayoutStatement()
{
	if (!expectSimpleToken(TokenType('(')))
		return nullptr;

	const Token layoutNameToken = tokenizer.getToken();
	if (layoutNameToken.type != TokenType::Identifier)
	{
		reportError("expected pipeline layout name", layoutNameToken);
		return nullptr;
	}

	if (!expectSimpleToken(TokenType(')')))
		return nullptr;

	PipelineLayout* pipelineLayout = pipelineLayoutList.find(layoutNameToken.string);
	if (!pipelineLayout)
	{
		reportError("undefined pipeline layout", layoutNameToken);
		return nullptr;
	}

	return pipelineLayout;
}

Shader* BuildDescriptionLoader::parseSetShaderStatement(
	HAL::ShaderCompiler::ShaderType shaderType, PipelineLayout& pipelineLayout)
{
	if (!expectSimpleToken(TokenType('(')))
		return nullptr;

	const Token pathToken = tokenizer.getToken();
	if (pathToken.type != TokenType::StringLiteral)
	{
		reportError("expected source path string literal", pathToken);
		return nullptr;
	}

	if (!expectSimpleToken(TokenType(',')))
		return nullptr;

	const Token entryPointNameToken = tokenizer.getToken();
	if (entryPointNameToken.type != TokenType::StringLiteral)
	{
		reportError("expected entry point name string literal", entryPointNameToken);
		return nullptr;
	}

	if (!expectSimpleToken(TokenType(')')))
		return nullptr;

	const SourceCreationResult sourceCreationResult = sourceCache.findOrCreate(pathToken.string);

	if (sourceCreationResult.status == SourceCreationStatus::Failure_PathIsTooLong)
	{
		reportError("source path is too long", pathToken);
		return nullptr;
	}
	else if (sourceCreationResult.status == SourceCreationStatus::Failure_InvalidPath)
	{
		reportError("source path is invalid", pathToken);
		return nullptr;
	}

	XAssert(sourceCreationResult.status == SourceCreationStatus::Success);
	XAssert(sourceCreationResult.source);

	const ShaderCreationResult shaderCreationResult =
		shaderList.findOrCreate(shaderType, *sourceCreationResult.source, entryPointNameToken.string, pipelineLayout);

	if (shaderCreationResult.status == ShaderCreationStatus::Failure_ShaderTypeMismatch)
	{
		reportError("same shader is already defined with other type", pathToken);
		return nullptr;
	}

	XAssert(shaderCreationResult.status == ShaderCreationStatus::Success);
	XAssert(shaderCreationResult.shader);

	return shaderCreationResult.shader;
}

void BuildDescriptionLoader::reportError(const char* message, const Token& token)
{
	TextWriteFmtStdOut(path, ':', token.line + 1, ':', token.offset + 1, ": error: ", message, '\n');
}

BuildDescriptionLoader::BuildDescriptionLoader(
	DescriptorSetLayoutList& descriptorSetLayoutList,
	PipelineLayoutList& pipelineLayoutList,
	PipelineList& pipelineList,
	ShaderList& shaderList,
	SourceCache& sourceCache)
	:
	descriptorSetLayoutList(descriptorSetLayoutList),
	pipelineLayoutList(pipelineLayoutList),
	pipelineList(pipelineList),
	shaderList(shaderList),
	sourceCache(sourceCache)
{}

bool BuildDescriptionLoader::load(const char* pathCStr)
{
	path = pathCStr;

	DynamicStringASCII text;

	// Read text from file.
	{
		File file;
		file.open(pathCStr, FileAccessMode::Read, FileOpenMode::OpenExisting);
		if (!file.isInitialized())
			return false;

		// TODO: Check for U32 overflow.
		const uint32 fileSize = uint32(file.getSize());

		text.resizeUnsafe(fileSize);
		file.read(text.getData(), fileSize);
		file.close();
	}

	tokenizer.reset(text.getData(), text.getLength());

	for (;;)
	{
		const Token token = tokenizer.getToken();
		if (token.type == TokenType::EndOfText)
			return true;
		if (token.type == TokenType::Semicolon)
			continue;

		if (token.type != TokenType::Identifier)
		{
			reportError("expected declaration", token);
			return false;
		}

		if (String::IsEqual(token.string, "DescriptorSetLayout"))
		{
			if (!parseDescriptorSetLayoutDeclaration())
				return false;
		}
		else if (String::IsEqual(token.string, "PipelineLayout"))
		{
			if (!parsePipelineLayoutDeclaration())
				return false;
		}
		else if (String::IsEqual(token.string, "GraphicsPipeline"))
		{
			if (!parseGraphicsPipelineDeclaration())
				return false;
		}
		else if (String::IsEqual(token.string, "ComputePipeline"))
		{
			if (!parseComputePipelineDeclaration())
				return false;
		}
		else
		{
			reportError("unknown keyword", token);
			return false;
		}
	}

	return true;
}
