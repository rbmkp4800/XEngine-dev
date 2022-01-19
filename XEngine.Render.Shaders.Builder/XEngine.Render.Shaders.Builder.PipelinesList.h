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
		~Pipeline() = default;

	public:
		inline Pipeline(const PipelineLayout& pipelineLayout) : pipelineLayout(pipelineLayout) {}

		bool compile();

		inline const char* getName() const { return name; }
		inline uint64 getNameCRC() const { return nameCRC; }
		inline const PipelineLayout& getPipelineLayout() const { return pipelineLayout; }
		inline bool isGraphicsPipeline() const { return isGraphics; }
		inline const HAL::ShaderCompiler::CompiledShader& getCompiledCompute() const { return computeShader->getCompiled(); }
		inline const HAL::ShaderCompiler::CompiledGraphicsPipeline& getCompiledGraphics() const { return compiledGraphicsPipeline; }
	};

	class PipelinesList : public XLib::NonCopyable
	{
	private:
		struct EntriesSearchTreeComparator;

		using EntriesSearchTree = XLib::IntrusiveBinaryTree<Pipeline, &Pipeline::searchTreeHook, EntriesSearchTreeComparator>;
		using EntriesStorageList = XLib::FixedLogSegmentedArrayList<Pipeline, 5, 16>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;

	private:
		Pipeline* createPipelineInternal(const char* name, const PipelineLayout& pipelineLayout,
			const GraphicsPipelineDesc* graphicsPipelineDesc, Shader* computeShader);

	public:
		using Iterator = EntriesSearchTree::Iterator;

	public:
		PipelinesList() = default;
		~PipelinesList() = default;

		Pipeline* createGraphicsPipeline(const char* name, const PipelineLayout& pipelineLayout, const GraphicsPipelineDesc& pipelineDesc) { return createPipelineInternal(name, pipelineLayout, &pipelineDesc, nullptr); }
		Pipeline* createComputePipeline(const char* name, const PipelineLayout& pipelineLayout, Shader& computeShader) { return createPipelineInternal(name, pipelineLayout, nullptr, &computeShader); }

		inline uint32 getSize() const { return entriesStorageList.getSize(); }

		inline Iterator begin() { return entriesSearchTree.begin(); }
		inline Iterator end() { return entriesSearchTree.end(); }
	};

	struct PipelinesList::EntriesSearchTreeComparator abstract final
	{
		static inline ordering Compare(const Pipeline& left, const Pipeline& right) { return compare(left.nameCRC, right.nameCRC); }
		static inline ordering Compare(const Pipeline& left, uint64 right) { return compare(left.nameCRC, right); }
	};
}
