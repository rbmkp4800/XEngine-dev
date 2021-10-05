#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

namespace XEngine::Render::Shaders::Builder
{
	class BindingLayoutsList;

	enum class BindPointType : uint8
	{
		None = 0,
		Constants,
		ConstantBuffer,
		ReadOnlyBuffer,
		ReadWriteBuffer,
	};

	struct BindPointDesc
	{
		XLib::InplaceString<28, uint8> name;
		BindPointType type;
		uint8 constantCount;
		uint8 shaderVisibility;
	};

	class BindingLayout : public XLib::NonCopyable
	{
		friend BindingLayoutsList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		XLib::InplaceString<63, uint8> name;

		XLib::InplaceArrayList<BindPointDesc, 32, uint8, false> bindPoints;

	private:
		BindingLayout() = default;
		~BindingLayout() = default;

	public:

	};

	class BindingLayoutsList : public XLib::NonCopyable
	{
	private:
		using EntriesSearchTree = XLib::IntrusiveBinaryTree<BindingLayout, &BindingLayout::searchTreeHook>;
		using EntriesStorageList = XLib::StaticSegmentedArrayList<BindingLayout, 4, 10>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;

	public:
		BindingLayoutsList() = default;
		~BindingLayoutsList() = default;
	};
}
