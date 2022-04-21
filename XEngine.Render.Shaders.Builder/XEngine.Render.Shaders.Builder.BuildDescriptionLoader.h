#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.Common.h>

#include "PseudoCTokenizer.h"
#include "XEngine.Render.Shaders.Builder.PipelineLayoutList.h"
#include "XEngine.Render.Shaders.Builder.PipelineList.h"
#include "XEngine.Render.Shaders.Builder.ShaderList.h"
#include "XEngine.Render.Shaders.Builder.SourceCache.h"

namespace XEngine::Render::Shaders::Builder_
{
	class BuildDescriptionLoader : public XLib::NonCopyable
	{
	private:
		using Tokenizer = ::PseudoCTokenizer;
		using TokenType = ::PseudoCTokenizer::TokenType;
		using Token = ::PseudoCTokenizer::Token;

	private:
		// These return not valid format on failure.
		static HAL::TexelViewFormat ParseTexelViewFormatString(XLib::StringViewASCII string);
		static HAL::DepthStencilFormat ParseDepthStencilFormatString(XLib::StringViewASCII string);

	private:
		Tokenizer tokenizer;

		PipelineLayoutList& pipelineLayoutList;
		PipelineList& pipelineList;
		ShaderList& shaderList;
		SourceCache& sourceCache;

	private:
		bool expectSimpleToken(TokenType type);
		bool parsePipelineLayoutDeclaration();
		bool parseGraphicsPipelineDeclaration();
		bool parseComputePipelineDeclaration();

		PipelineLayout* parseSetLayoutStatement();
		Shader* parseSetShaderStatement(HAL::ShaderCompiler::ShaderType shaderType, PipelineLayout& pipelineLayout);

		void reportError(const char* message);

	public:
		BuildDescriptionLoader(
			PipelineLayoutList& pipelineLayoutList,
			PipelineList& pipelineList,
			ShaderList& shaderList,
			SourceCache& sourceCache);
		~BuildDescriptionLoader() = default;

		bool load(const char* pathCStr);
	};
}
