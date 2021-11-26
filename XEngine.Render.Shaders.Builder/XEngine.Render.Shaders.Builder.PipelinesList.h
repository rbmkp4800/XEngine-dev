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
		Shader* vertexShader;
		Shader* amplificationShader;
		Shader* meshShader;
		Shader* pixelShader;
		HAL::TexelViewFormat renderTargetsFormats[HAL::MaxRenderTargetCount];
		HAL::DepthStencilFormat depthStencilFormat;
	};

	class Pipeline : public XLib::NonCopyable
	{
		friend PipelinesList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;
		const char* name = nullptr;
		uint64 nameCRC = 0;

		const PipelineLayout& pipelineLayout;
		Shader* computeShader = nullptr;
		GraphicsPipelineDesc graphicsDesc = {};
		bool isGraphics = false;

		HAL::ShaderCompiler::CompiledGraphicsPipeline compiledGraphicsPipeline;

	private:
		Pipeline() = default;
		~Pipeline() = default;

	public:
		bool compile();

		inline const char* getName() const { return name; }
		inline uint64 getNameCRC() const { return nameCRC; }
		inline const PipelineLayout& getPipelineLayout() const { return pipelineLayout; }
		inline bool isGraphics() const { return isGraphics; }
		inline const HAL::ShaderCompiler::CompiledShader& getCompiledCompute() const { return computeShader->getCompiled(); }
		inline const HAL::ShaderCompiler::CompiledGraphicsPipeline& getCompiledGraphics() const { return compiledGraphicsPipeline; }
	};

	class PipelinesList : public XLib::NonCopyable
	{
	private:
		using EntriesOrderedSearchTree = XLib::IntrusiveBinaryTree<Pipeline, &Pipeline::searchTreeHook>;
		using EntriesStorageList = XLib::StaticSegmentedArrayList<Pipeline, 5, 16>;

	private:
		EntriesOrderedSearchTree entriesOrderedSearchTree;
		EntriesStorageList entriesStorageList;

	public:
		using Iterator = EntriesOrderedSearchTree::Iterator;

	public:
		PipelinesList() = default;
		~PipelinesList() = default;

		Pipeline* createGraphicsPipeline(const char* name, const PipelineLayout& pipelineLayout, const GraphicsPipelineDesc& pipelineDesc);
		Pipeline* createComputePipeline(const char* name, const PipelineLayout& pipelineLayout, const Shader& computeShader);

		inline uint32 getSize() const;

		inline Iterator begin() { return entriesOrderedSearchTree.begin(); }
		inline Iterator end() { return entriesOrderedSearchTree.end(); }
	};
}
