#include <XLib.Containers.ArrayList.h>
#include <XLib.String.h>
#include <XLib.Text.h>

#include <XEngine.Render.HAL.Common.h>
#include <XEngine.Render.HAL.ShaderCompiler.h>

#include "XEngine.Render.Shaders.Builder.TargetDescriptionLoader.h"

#include "XEngine.Render.Shaders.Builder.PipelineLayoutsList.h"
#include "XEngine.Render.Shaders.Builder.PipelinesList.h"
#include "XEngine.Render.Shaders.Builder.ShadersList.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

using namespace XLib;
using namespace XEngine::Render;
using namespace XEngine::Render::Shaders::Builder_;

HAL::TexelViewFormat TargetDescriptionLoader::ParseTexelViewFormatString(StringView string)
{
	// TODO: Do this properly.
	if (AreStringsEqual(string, "R8G8B8BA8_Unorm"))
		return HAL::TexelViewFormat::R8G8B8A8_UNORM;

	return HAL::TexelViewFormat(-1);
}

HAL::DepthStencilFormat TargetDescriptionLoader::ParseDepthStencilFormatString(StringView string)
{
	// TODO: Do this properly.
	if (AreStringsEqual(string, "D32"))
		return HAL::DepthStencilFormat::D32;

	return HAL::DepthStencilFormat(-1);
}

bool TargetDescriptionLoader::expectSimpleToken(TokenType type)
{
	const Token token = tokenizer.getToken();
	if (token.type != type)
	{
		reportError("expected token ..."); // TODO: Put expected token here.
		return false;
	}
	return true;
}

bool TargetDescriptionLoader::parsePipelineLayoutDeclaration()
{
	const Token pipelineLayoutNameToken = tokenizer.getToken();
	if (pipelineLayoutNameToken.type != TokenType::Identifier)
	{
		reportError("expected pipeline layout name");
		return false;
	}

	if (!expectSimpleToken(TokenType('{')))
		return false;

	InplaceArrayList<BindPointDesc, HAL::MaxPipelineBindPointCount> bindPoints;

	for (;;)
	{
		const Token headToken = tokenizer.getToken();
		if (headToken.type == TokenType('}'))
			break;
		if (headToken.type == PseudoCTokenizer::TokenType::Semicolon)
			continue;

		BindPointDesc bindPointDesc = {};

		if (headToken.type == TokenType::Keyword_ReadOnlyBuffer)
			bindPointDesc.type = HAL::PipelineBindPointType::ReadOnlyBuffer;

		const Token bindPointNameToken = tokenizer.getToken();
		if (bindPointNameToken.type != TokenType::Identifier)
		{
			reportError("expected bind point name");
			return false;
		}

		bindPointDesc.name = bindPointNameToken.string;

		if (!expectSimpleToken(TokenType(';')))
			return false;

		if (bindPoints.isFull())
		{
			reportError("too many bind points");
			return false;
		}

		bindPoints.pushBack(bindPointDesc);
	}

	const PipelineLayoutsList::EntryCreationResult pipelineLayoutCreationResult =
		pipelineLayoutsList.createEntry(pipelineLayoutNameToken.string, bindPoints, uint8(bindPoints.getSize()));
	
	if (pipelineLayoutCreationResult.status != PipelineLayoutsList::EntryCreationStatus::Success)
	{
		// TODO: Proper error handling (CRC collsitions etc).
		reportError("pipeline layout redefinition");
		return false;
	}

	return true;
}

bool TargetDescriptionLoader::parseGraphicsPipelineDeclaration()
{
	const Token pipelineNameToken = tokenizer.getToken();
	if (pipelineNameToken.type != TokenType::Identifier)
	{
		reportError("expected pipeline name");
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
		const Token statementToken = tokenizer.getToken();
		if (statementToken.type == TokenType('}'))
			break;
		if (statementToken.type == PseudoCTokenizer::TokenType::Semicolon)
			continue;

		if (statementToken.type == TokenType::Keyword_SetLayout)
		{
			if (pipelineLayout)
			{
				reportError("pipeline layout already defined");
				return false;
			}

			pipelineLayout = parseSetLayoutStatement();
			if (!pipelineLayout)
				return false;
		}
		else if (statementToken.type == TokenType::Keyword_SetVS)
		{
			if (pipelineDesc.vertexShader)
			{
				reportError("vertex shader already defined");
				return false;
			}
			if (!pipelineLayout)
			{
				reportError("pipeline layout should be defined prior to shaders");
				return false;
			}

			pipelineDesc.vertexShader = parseSetShaderStatement(HAL::ShaderCompiler::ShaderType::Vertex, *pipelineLayout);
			if (!pipelineDesc.vertexShader)
				return false;
		}
		else if (statementToken.type == TokenType::Keyword_SetPS)
		{
			if (pipelineDesc.pixelShader)
			{
				reportError("pixel shader already defined");
				return false;
			}
			if (!pipelineLayout)
			{
				reportError("pipeline layout should be defined prior to shaders");
				return false;
			}

			pipelineDesc.pixelShader = parseSetShaderStatement(HAL::ShaderCompiler::ShaderType::Pixel, *pipelineLayout);
			if (!pipelineDesc.pixelShader)
				return false;
		}
		else if (statementToken.type == TokenType::Keyword_SetRT)
		{
			if (!expectSimpleToken(TokenType('(')))
				return false;

			const Token rtIndexToken = tokenizer.getToken();
			if (rtIndexToken.type != TokenType::NumberLiteral)
			{
				reportError("expected render target index");
				return false;
			}
			if (rtIndexToken.number < 0 || rtIndexToken.number >= HAL::MaxRenderTargetCount)
			{
				reportError("invalid render target index");
				return false;
			}

			const uint8 rtIndex = uint8(rtIndexToken.number);

			const bool rtIsAlreadySet = (setRTsMask & (1 << rtIndex)) != 0;
			if (rtIsAlreadySet)
			{
				reportError("this render target is already set");
				return false;
			}

			if (!expectSimpleToken(TokenType(',')))
				return false;

			const Token rtFormatToken = tokenizer.getToken();
			if (rtFormatToken.type != TokenType::Identifier)
			{
				reportError("expected render target format identifier");
				return false;
			}

			const HAL::TexelViewFormat rtFormat = ParseTexelViewFormatString(rtFormatToken.string);
			if (!HAL::ValidateTexelViewFormatValue(rtFormat))
			{
				reportError("unknown format");
				return false;
			}

			if (!HAL::ValidateTexelViewFormatForRenderTargetUsage(rtFormat))
			{
				reportError("this format is not supported for render target usage");
				return false;
			}

			if (!expectSimpleToken(TokenType(')')))
				return false;

			pipelineDesc.renderTargetsFormats[rtIndex] = rtFormat;
			setRTsMask |= 1 << rtIndex;
		}
		else if (statementToken.type == TokenType::Keyword_SetDepthRT)
		{
			if (depthRTIsSet)
			{
				reportError("depth render target is already set");
				return false;
			}

			if (!expectSimpleToken(TokenType('(')))
				return false;

			const Token depthRTFormatToken = tokenizer.getToken();
			if (depthRTFormatToken.type != TokenType::Identifier)
			{
				reportError("expected depth render target format identifier");
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
			reportError("expected graphics pipeline setup statement");
			return false;
		}

		if (!expectSimpleToken(TokenType(';')))
			return false;
	}

	if (!pipelineLayout)
	{
		reportError("pipeline layout is not defined");
		return false;
	}

	// TODO: More validation (shader combinations etc).

	const PipelinesList::EntryCreationResult pipelineCreationResult =
		pipelinesList.createEntry(pipelineNameToken.string, *pipelineLayout, &pipelineDesc, nullptr);
	if (pipelineCreationResult.status != PipelinesList::EntryCreationStatus::Success)
	{
		// TODO: Proper error handling (CRC collsitions etc).
		reportError("pipeline with this name already defined");
		return false;
	}

	return true;
}

bool TargetDescriptionLoader::parseComputePipelineDeclaration()
{
	const Token pipelineNameToken = tokenizer.getToken();
	if (pipelineNameToken.type != TokenType::Identifier)
	{
		reportError("expected pipeline name");
		return false;
	}

	if (!expectSimpleToken(TokenType('{')))
		return false;

	PipelineLayout* pipelineLayout = nullptr;
	Shader* computeShader = nullptr;

	for (;;)
	{
		const Token statementToken = tokenizer.getToken();
		if (statementToken.type == TokenType('}'))
			break;
		if (statementToken.type == PseudoCTokenizer::TokenType::Semicolon)
			continue;

		if (statementToken.type == TokenType::Keyword_SetLayout)
		{
			if (pipelineLayout)
			{
				reportError("pipeline layout already defined");
				return false;
			}

			pipelineLayout = parseSetLayoutStatement();
			if (!pipelineLayout)
				return false;
		}
		else if (statementToken.type == TokenType::Keyword_SetCS)
		{
			if (computeShader)
			{
				reportError("compute shader already defined");
				return false;
			}
			if (!pipelineLayout)
			{
				reportError("pipeline layout should be defined prior to shaders");
				return false;
			}

			computeShader = parseSetShaderStatement(HAL::ShaderCompiler::ShaderType::Compute, *pipelineLayout);
			if (!computeShader)
				return false;
		}
		else
		{
			reportError("expected compute pipeline setup statement");
			return false;
		}

		if (!expectSimpleToken(TokenType(';')))
			return false;
	}

	if (!pipelineLayout)
	{
		reportError("pipeline layout is not defined");
		return false;
	}

	if (!computeShader)
	{
		reportError("compute shader is not defined");
		return false;
	}

	const PipelinesList::EntryCreationResult pipelineCreationResult =
		pipelinesList.createEntry(pipelineNameToken.string, *pipelineLayout, nullptr, computeShader);
	if (pipelineCreationResult.status != PipelinesList::EntryCreationStatus::Success)
	{
		// TODO: Proper error handling (CRC collsitions etc).
		reportError("pipeline with this name already defined");
		return false;
	}

	return true;
}

PipelineLayout* TargetDescriptionLoader::parseSetLayoutStatement()
{
	if (!expectSimpleToken(TokenType('(')))
		return nullptr;

	const Token layoutNameToken = tokenizer.getToken();
	if (layoutNameToken.type != TokenType::Identifier)
	{
		reportError("expected pipeline layout name");
		return nullptr;
	}

	if (!expectSimpleToken(TokenType(')')))
		return nullptr;

	PipelineLayout *pipelineLayout = pipelineLayoutsList.findEntry(layoutNameToken.string);
	if (!pipelineLayout)
	{
		reportError("pipeline layout is not found");
		return nullptr;
	}

	return pipelineLayout;
}

Shader* TargetDescriptionLoader::parseSetShaderStatement(
	HAL::ShaderCompiler::ShaderType shaderType, PipelineLayout& pipelineLayout)
{
	if (!expectSimpleToken(TokenType('(')))
		return nullptr;

	const Token pathToken = tokenizer.getToken();
	if (pathToken.type != TokenType::StringLiteral)
	{
		reportError("expected source path string literal");
		return nullptr;
	}

	if (!expectSimpleToken(TokenType(',')))
		return nullptr;

	const Token entryPointNameToken = tokenizer.getToken();
	if (pathToken.type != TokenType::StringLiteral)
	{
		reportError("expected entry point name string literal");
		return nullptr;
	}

	if (!expectSimpleToken(TokenType(')')))
		return nullptr;

	const SourcesCache::EntryCreationResult sourceCreationResult = sourcesCache.createEntryIfAbsent(pathToken.string);

	if (sourceCreationResult.status == SourcesCache::EntryCreationStatus::Failure_PathIsTooLong)
	{
		reportError("source path is too long");
		return nullptr;
	}
	else if (sourceCreationResult.status == SourcesCache::EntryCreationStatus::Failure_InvalidPath)
	{
		reportError("source path is invalid");
		return nullptr;
	}

	XAssert(sourceCreationResult.status == SourcesCache::EntryCreationStatus::Success);
	XAssert(sourceCreationResult.entry);

	const ShadersList::EntryCreationResult shaderCreationResult =
		shadersList.createEntryIfAbsent(shaderType, *sourceCreationResult.entry, entryPointNameToken.string, pipelineLayout);

	if (shaderCreationResult.status == ShadersList::EntryCreationStatus::Failure_ShaderAlreadyCreatedWithOtherType)
	{
		reportError("same shader is already defined with other type");
		return nullptr;
	}

	XAssert(shaderCreationResult.status == ShadersList::EntryCreationStatus::Success);
	XAssert(shaderCreationResult.entry);

	return shaderCreationResult.entry;
}

void TargetDescriptionLoader::reportError(const char* message)
{
	TextWriteFmtStdOut(message);
}

bool TargetDescriptionLoader::parse()
{
	for (;;)
	{
		PseudoCTokenizer::Token token = tokenizer.getToken();
		if (token.type == PseudoCTokenizer::TokenType::EndOfText)
			return true;
		if (token.type == PseudoCTokenizer::TokenType::Semicolon)
			continue;

		if (token.type == PseudoCTokenizer::TokenType::Keyword_PipelineLayout)
		{
			if (!parsePipelineLayoutDeclaration())
				return false;
		}
		else if (token.type == PseudoCTokenizer::TokenType::Keyword_GraphicsPipeline)
		{
			if (!parseGraphicsPipelineDeclaration())
				return false;
		}
		else if (token.type == PseudoCTokenizer::TokenType::Keyword_ComputePipeline)
		{
			if (!parseComputePipelineDeclaration())
				return false;
		}
		else
		{
			reportError("expected declaration");
			return false;
		}
	}

	return true;
}
