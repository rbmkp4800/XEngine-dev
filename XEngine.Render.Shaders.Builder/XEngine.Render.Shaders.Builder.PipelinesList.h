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
		XLib::InplaceString256 name;
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

		inline XLib::StringView getName() const { return name; }
		//inline uint64 getNameCRC() const { return nameCRC; }
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


	public:
		using Iterator = EntriesSearchTree::Iterator;

		enum class EntryCreationStatus
		{
			Success = 0,
			Failure_EntryWithSuchNameAlreadyExists,
			//Failure_EntryNameCRCCollision,
		};

		struct EntryCreationResult
		{
			Pipeline* entry;
			EntryCreationStatus status;
		};

	public:
		PipelinesList() = default;
		~PipelinesList() = default;

		EntryCreationResult createPipeline(XLib::StringView name, const PipelineLayout& pipelineLayout,
			const GraphicsPipelineDesc* graphicsPipelineDesc, Shader* computeShader);

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
