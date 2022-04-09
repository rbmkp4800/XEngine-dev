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

		XLib::StringView name;
		uint64 nameCRC = 0;

		const PipelineLayout* pipelineLayout = nullptr;
		Shader* computeShader = nullptr;
		GraphicsPipelineDesc graphicsDesc = {};
		bool isGraphics = false;

		HAL::ShaderCompiler::CompiledGraphicsPipeline compiledGraphicsPipeline;

	private:
		Pipeline() = default;
		~Pipeline() = default;

	public:
		bool compile();

		inline XLib::StringView getName() const { return name; }
		inline uint64 getNameCRC() const { return nameCRC; }
		inline const PipelineLayout& getPipelineLayout() const { return *pipelineLayout; }
		inline bool isGraphicsPipeline() const { return isGraphics; }

		inline const HAL::ShaderCompiler::CompiledShader& getCompiledCompute() const { return computeShader->getCompiled(); }
		inline const HAL::ShaderCompiler::CompiledGraphicsPipeline& getCompiledGraphics() const { return compiledGraphicsPipeline; }
	};

	enum class PipelineCreationStatus
	{
		Success = 0,
		Failure_EntryNameDuplication,
		//Failure_EntryNameCRCCollision,
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
			static inline ordering Compare(const Pipeline& left, const Pipeline& right) { return compare(left.nameCRC, right.nameCRC); }
			static inline ordering Compare(const Pipeline& left, uint64 right) { return compare(left.nameCRC, right); }
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

		PipelineCreationResult create(XLib::StringView name, const PipelineLayout& pipelineLayout,
			const GraphicsPipelineDesc* graphicsPipelineDesc, Shader* computeShader);

		inline uint32 getSize() const { return entryCount; }

		inline Iterator begin() { return entrySearchTree.begin(); }
		inline Iterator end() { return entrySearchTree.end(); }
	};
}
