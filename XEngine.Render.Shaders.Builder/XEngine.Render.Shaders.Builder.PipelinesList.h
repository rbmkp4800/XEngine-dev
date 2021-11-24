#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.Common.h>

#include "XEngine.Render.Shaders.Builder.PipelineLayoutsList.h"
#include "XEngine.Render.Shaders.Builder.ShadersList.h"

namespace XEngine::Render::Shaders::Builder_
{
	class PipelinesList;

	struct GraphicsPipelineDesc
	{
		ShaderRef vertexShader;
		ShaderRef amplificationShader;
		ShaderRef meshShader;
		ShaderRef pixelShader;
		HAL::TexelFormat renderTargetsFormats[HAL::MaxRenderTargetCount];
		HAL::TexelFormat depthStencilFormat;
	};

	class Pipeline : public XLib::NonCopyable
	{
		friend PipelinesList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		XLib::InplaceString<63, uint8> name;
		uint64 nameCRC = 0;
		PipelineLayoutRef pipelineLayout = ZeroPipelineLayoutRef;

		ShaderRef computeShader = ZeroShaderRef;
		GraphicsPipelineDesc graphicsDesc = {};
		bool isGraphics = false;

		HAL::ShaderCompiler::CompiledShader compiledComputeShader;
		HAL::ShaderCompiler::CompiledGraphicsPipeline compiledGraphicsPipeline;

	private:
		Pipeline() = default;
		~Pipeline() = default;

	public:
		bool compile();

		inline uint64 getNameCRC() const { return nameCRC; }
		inline PipelineLayoutRef getPipelineLayout() const { return pipelineLayout; }
		inline bool isGraphics() const { return isGraphics; }
		inline const HAL::ShaderCompiler::CompiledShader& getCompiledCompute() const { return compiledComputeShader; }
		inline const HAL::ShaderCompiler::CompiledGraphicsPipeline& getCompiledGraphics() const { return compiledGraphicsPipeline; }
	};

	class PipelinesList : public XLib::NonCopyable
	{
	private:
		using EntriesSearchTree = XLib::IntrusiveBinaryTree<Pipeline, &Pipeline::searchTreeHook>;
		using EntriesStorageList = XLib::StaticSegmentedArrayList<Pipeline, 5, 16>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;

	public:
		PipelinesList() = default;
		~PipelinesList() = default;

		Pipeline* createGraphicsPipeline(const char* name, PipelineLayoutRef pipelineLayout, const GraphicsPipelineDesc& pipelineDesc);
		Pipeline* createComputePipeline(const char* name, PipelineLayoutRef pipelineLayout, ShaderRef computeShader);

		inline uint32 getSize() const;
	};
}
