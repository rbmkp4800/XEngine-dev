#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include "XEngine.Render.Shaders.Builder.PipelineLayoutsList.h"
#include "XEngine.Render.Shaders.Builder.ShadersList.h"

namespace XEngine::Render::Shaders::Builder_
{
	class PipelinesList;

	enum class PipelineConfig : uint8
	{
		None = 0,
		Compute,
		GraphicsMesh,
	};

	class Pipeline : public XLib::NonCopyable
	{
		friend PipelinesList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		XLib::InplaceString<63, uint8> name;
		uint64 nameCRC = 0;
		PipelineLayoutRef PipelineLayout = ZeroPipelineLayoutRef;

		PipelineConfig type = PipelineConfig::None;
		ShaderRef cs;
		ShaderRef vs;
		ShaderRef ps;

		HAL::ShaderCompiler::CompiledPipeline compiledPipeline;

	private:
		Pipeline() = default;
		~Pipeline() = default;

	public:
		inline uint64 getNameCRC() const;
		inline const HAL::ShaderCompiler::CompiledPipeline& getCompiled() const { return compiledPipeline; }
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

		inline uint32 getSize() const;
	};
}
