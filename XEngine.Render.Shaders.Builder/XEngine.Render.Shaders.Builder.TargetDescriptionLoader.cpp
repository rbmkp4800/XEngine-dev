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
	if (!matchSimpleToken(TokenType('(')))
		return false;

	InplaceString2048 pipelineLayoutName;
	if (!ReadStringLiteral(tokenizer, pipelineLayoutName, 1024))
		return false;

	if (!matchSimpleToken(TokenType(')')))
		return false;
	if (!matchSimpleToken(TokenType('{')))
		return false;



	InplaceArrayList<BindPointDesc, HAL::MaxPipelineBindPointCount> bindPoints;

	for (;;)
	{
		Token token = tokenizer.getToken();

		if (token.type == TokenType('}'))
			break;

		if (token.type != TokenType::Identifier)
		{
			reportError("expected statement");
			return false;
		}

		BindPointDesc bindPointDesc = {};

		if (AreStringsEqual(token.string, "AddConstantBuffer"))
		{
			bindPointDesc.type = HAL::PipelineBindPointType::ConstantBuffer;
		}

		if (!matchSimpleToken(TokenType('(')))
			return false;



		if (!matchSimpleToken(TokenType(')')))
			return false;
		if (!matchSimpleToken(TokenType(';')))
			return false;

		if (bindPoints.isFull())
		{
			reportError("too many bind points");
			return false;
		}

		bindPoints.pushBack(bindPointDesc);
	}

	pipelineLayoutsList.createEntry("TestPipelineLayout", bindPoints, bindPoints.getSize());
}

bool TargetDescriptionLoader::parse()
{
	for (;;)
	{
		PseudoCTokenizer::Token token = tokenizer.getToken();
		if (token.type == PseudoCTokenizer::TokenType::EndOfText)
			return true;

		if (token.type != PseudoCTokenizer::TokenType::Identifier)
		{
			reportError("expected declaration");
			return false;
		}

		if (AreStringsEqual(token.string, "DeclarePipelineLayout"))
		{
			if (!parsePipelineLayoutDeclaration())
				return false;
		}
		else if (AreStringsEqual(token.string, "DeclareGraphicsPipeline"))
		{
			if (!parseGraphicsPipelineDeclaration())
				return false;
		}
		else if (AreStringsEqual(token.string, "DeclareComputePipeline"))
		{
			if (!parseComputePipelineDeclaration())
				return false;
		}
		else
		{
			reportError("unknown keyword");
			return false;
		}
	}
}
