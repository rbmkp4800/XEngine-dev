#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include "PseudoCTokenizer.h"

namespace XEngine::Render::Shaders::Builder_
{
	class PipelineLayout;
	class Shader;

	class PipelineLayoutsList;
	class PipelinesList;
	class ShadersList;
	class SourcesCache;

	class TargetDescriptionLoader : public XLib::NonCopyable
	{
	private:
		using Tokenizer = PseudoCTokenizer;
		using TokenType = PseudoCTokenizer::TokenType;
		using Token = PseudoCTokenizer::Token;

	private:
		// These return not valid format on failure.
		static HAL::TexelViewFormat ParseTexelViewFormatString(XLib::StringView string);
		static HAL::DepthStencilFormat ParseDepthStencilFormatString(XLib::StringView string);

	private:
		Tokenizer tokenizer;

		PipelineLayoutsList& pipelineLayoutsList;
		PipelinesList& pipelinesList;
		ShadersList& shadersList;
		SourcesCache& sourcesCache;

	private:
		bool matchSimpleToken(TokenType type);
		bool parsePipelineLayoutDeclaration();
		bool parseGraphicsPipelineDeclaration();
		bool parseComputePipelineDeclaration();
		PipelineLayout* parseSetLayoutStatement();
		Shader* parseSetShaderStatement(HAL::ShaderCompiler::ShaderType shaderType, PipelineLayout& pipelineLayout);

		void reportError(const char* message);

	public:
		TargetDescriptionLoader(const char* targetDescriptionPath);
		~TargetDescriptionLoader() = default;

		bool parse();
	};
}