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
using namespace XEngine::Render::Shaders::Builder_;

inline bool ReadStringLiteral(PseudoCTokenizer& tokenizer, InplaceString2048& resultString, uint16 lengthLimit)
{
	XAssert(lengthLimit < InplaceString2048::GetCapacity());
}

bool TargetDescriptionLoader::matchSimpleToken(TokenType type)
{

}

bool TargetDescriptionLoader::parsePipelineLayoutDeclaration()
{
	const Token pipelineLayoutNameToken = tokenizer.getToken();
	if (pipelineLayoutNameToken.type != TokenType::Identifier)
	{
		reportError("expected pipeline layout name");
		return false;
	}

	if (!matchSimpleToken(TokenType('{')))
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

		if (!matchSimpleToken(TokenType(';')))
			return false;

		if (bindPoints.isFull())
		{
			reportError("too many bind points");
			return false;
		}

		bindPoints.pushBack(bindPointDesc);
	}

	PipelineLayout* pipelineLayout = pipelineLayoutsList.createEntry(
		pipelineLayoutNameToken.string, bindPoints, bindPoints.getSize());
	
	if (!pipelineLayout)
	{
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

	if (!matchSimpleToken(TokenType('{')))
		return false;

	PipelineLayout* pipelineLayout = nullptr;
	GraphicsPipelineDesc pipelineDesc = {};

	for (;;)
	{
		const Token headToken = tokenizer.getToken();
		if (headToken.type == TokenType('}'))
			break;
		if (headToken.type == PseudoCTokenizer::TokenType::Semicolon)
			continue;

		if (headToken.type == TokenType::Keyword_SetLayout)
		{
			if (pipelineLayout)
			{
				reportError("pipeline layout already defined");
				return false;
			}

			if (!matchSimpleToken(TokenType('(')))
				return false;

			const Token layoutNameToken = tokenizer.getToken();
			if (layoutNameToken.type != TokenType::Identifier)
			{
				reportError("expected pipeline layout name");
				return false;
			}

			pipelineLayout = pipelineLayoutsList.findEntry(layoutNameToken.string);
			if (!pipelineLayout)
			{
				reportError("pipeline layout is not found");
				return false;
			}

			if (!matchSimpleToken(TokenType(')')))
				return false;
		}
		else if (headToken.type == TokenType::Keyword_SetVS)
		{

		}
		else if (headToken.type == TokenType::Keyword_SetPS)
		{

		}
		else if (headToken.type == TokenType::Keyword_SetRT)
		{

		}
		else if (headToken.type == TokenType::Keyword_SetDepthRT)
		{

		}
		else
		{
			reportError("expected graphics pipeline setup statement");
			return false;
		}

		if (!matchSimpleToken(TokenType(';')))
			return false;
	}

	if (!pipelineLayout)
	{
		reportError("pipeline layout is undefined");
		return false;
	}

	// TODO: More validation (shader combinations etc).

	pipelinesList.createGraphicsPipeline(pipelineNameToken.string, *pipelineLayout, pipelineDesc);
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
}
