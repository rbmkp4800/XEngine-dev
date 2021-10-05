#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include "XEngine.Render.Shaders.Builder.ShadersList.h"

namespace XEngine::Render::Shaders::Builder
{
	class PipelinesList;

	enum class PipelineType : uint8
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
		PipelineType type = PipelineType::None;

		ShaderId cs;
		ShaderId vs;
		ShaderId ps;

	private:
		Pipeline() = default;
		~Pipeline() = default;

	public:

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
