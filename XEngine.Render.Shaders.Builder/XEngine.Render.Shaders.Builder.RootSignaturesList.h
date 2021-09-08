#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

namespace XEngine::Render::Shaders::Builder
{
	class RootSignaturesList;

	using RootSignatureIndex = uint16;

	class RootSignature : public XLib::NonCopyable
	{
		friend RootSignaturesList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		XLib::InplaceString<63, uint8> name;

	private:
		RootSignature() = default;
		~RootSignature() = default;

	public:

	};

	class RootSignaturesList : public XLib::NonCopyable
	{
	private:
		using EntriesSearchTree = XLib::IntrusiveBinaryTree<RootSignature, &RootSignature::searchTreeHook>;
		using EntriesStorageList = XLib::StaticSegmentedArrayList<RootSignature, 4, 10>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;

	public:
		RootSignaturesList() = default;
		~RootSignaturesList() = default;
	};
}
