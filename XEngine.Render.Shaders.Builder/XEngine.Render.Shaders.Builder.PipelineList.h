#pragma once

#include <XLib.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.Common.h>
#include <XEngine.Render.HAL.ShaderCompiler.h>

#include "XEngine.Render.Shaders.Builder.PipelineLayoutList.h"
#include "XEngine.Render.Shaders.Builder.ShaderList.h"

namespace XEngine::Render::Shaders::Builder_
{
	class PipelineList;

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
		friend PipelineList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		XLib::StringViewASCII name;
		uint64 nameXSH = 0;

		const PipelineLayout* pipelineLayout = nullptr;
		Shader* computeShader = nullptr;
		GraphicsPipelineDesc graphicsDesc = {};
		bool isGraphics = false;

		HAL::ShaderCompiler::GraphicsPipelineCompilationResult compiledGraphicsPipeline;

	private:
		Pipeline() = default;
		~Pipeline() = default;

	public:
		bool compile();

		inline XLib::StringViewASCII getName() const { return name; }
		inline uint64 getNameXSH() const { return nameXSH; }
		inline const PipelineLayout& getPipelineLayout() const { return *pipelineLayout; }
		inline bool isGraphicsPipeline() const { return isGraphics; }

		inline const HAL::ShaderCompiler::Blob& getCompiledComputeShaderBlob() const { return computeShader->getCompiledBlob(); }
		inline const HAL::ShaderCompiler::GraphicsPipelineCompilationResult& getCompiledGraphics() const { return compiledGraphicsPipeline; }
	};

	enum class PipelineCreationStatus
	{
		Success = 0,
		Failure_EntryNameDuplication,
		//Failure_EntryNameHashCollision,
	};

	struct PipelineCreationResult
	{
		PipelineCreationStatus status;
		Pipeline* entry;
	};

	class PipelineList : public XLib::NonCopyable
	{
	private:
		struct EntrySearchTreeComparator abstract final
		{
			static inline ordering Compare(const Pipeline& left, const Pipeline& right) { return compare(left.nameXSH, right.nameXSH); }
			static inline ordering Compare(const Pipeline& left, uint64 right) { return compare(left.nameXSH, right); }
		};

		using EntrySearchTree =
			XLib::IntrusiveBinaryTree<Pipeline, &Pipeline::searchTreeHook, EntrySearchTreeComparator>;

	private:
		EntrySearchTree entrySearchTree;
		uint32 entryCount = 0;

	public:
		using Iterator = EntrySearchTree::Iterator;

	public:
		PipelineList() = default;
		~PipelineList() = default;

		PipelineCreationResult create(XLib::StringViewASCII name, const PipelineLayout& pipelineLayout,
			const GraphicsPipelineDesc* graphicsPipelineDesc, Shader* computeShader);

		inline uint32 getSize() const { return entryCount; }

		inline Iterator begin() { return entrySearchTree.begin(); }
		inline Iterator end() { return entrySearchTree.end(); }
	};
}
