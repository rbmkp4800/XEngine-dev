#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include "XEngine.Render.Shaders.Builder.BindingLayoutsList.h"
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
		BindingLayoutRef bindingLayout = InvalidBindingLayoutRef;

		PipelineConfig type = PipelineConfig::None;
		ShaderRef cs;
		ShaderRef vs;
		ShaderRef ps;

		XLib::InplaceArrayList<HAL::ShaderCompiler::BinaryBlob*, 8, uint8> compiledBlobs;

	private:
		Pipeline() = default;
		~Pipeline() = default;

	public:
		inline uint64 getNameCRC() const;

		inline XLib::ArrayView<HAL::ShaderCompiler::BinaryBlob*> getCompiledBlobs();
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
	};
}
