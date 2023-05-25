#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.Common.h>

#include "XJSON.h"
#include "XEngine.Render.Shaders.Builder.DescriptorSetLayoutList.h"
#include "XEngine.Render.Shaders.Builder.PipelineLayoutList.h"
#include "XEngine.Render.Shaders.Builder.PipelineList.h"
#include "XEngine.Render.Shaders.Builder.ShaderList.h"
#include "XEngine.Render.Shaders.Builder.SourceCache.h"

namespace XEngine::Render::Shaders::Builder_
{
	class ListingLoader : public XLib::NonCopyable
	{
	private:
		DescriptorSetLayoutList& descriptorSetLayoutList;
		PipelineLayoutList& pipelineLayoutList;
		PipelineList& pipelineList;
		ShaderList& shaderList;
		SourceCache& sourceCache;

		XJSON::MemoryStreamParser xjsonParser;
		const char* path = nullptr;

	private:
		void reportError(const char* message, const XJSON::Location& location);
		void reportXJSONError(const XJSON::ParserError& xjsonError);

		bool readDescriptorSetLayoutDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty);
		bool readPipelineLayoutDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty);
		bool readGraphicsPipelineDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty);
		bool readComputePipelineDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty);

		bool readPipelineLayoutSetupProperty(const XJSON::KeyValue& xjsonOuterProperty, PipelineLayout*& resultPipelineLayout);
		bool readShaderSetupProperty(const XJSON::KeyValue& xjsonOuterProperty,
			HAL::ShaderCompiler::ShaderType shaderType, PipelineLayout& pipelineLayout, Shader*& resultShader);

	public:
		ListingLoader(
			DescriptorSetLayoutList& descriptorSetLayoutList,
			PipelineLayoutList& pipelineLayoutList,
			PipelineList& pipelineList,
			ShaderList& shaderList,
			SourceCache& sourceCache);
		~ListingLoader() = default;

		bool load(const char* pathCStr);
	};
}
