#pragma once

#include <XLib.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

namespace XEngine::Render::Shaders::Builder_
{
	class BindingLayoutsList;

	using BindingLayoutRef = uint16;
	static constexpr BindingLayoutRef InvalidBindingLayoutRef = BindingLayoutRef(0);

	class BindingLayout : public XLib::NonCopyable
	{
		friend BindingLayoutsList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		XLib::InplaceString<63, uint8> name;
		HAL::ShaderCompiler::CompiledBindingLayout compiled;

	private:
		BindingLayout() = default;
		~BindingLayout() = default;

	public:
		inline const HAL::ShaderCompiler::CompiledBindingLayout& getCompiled() const { return compiled; }
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

		BindingLayoutRef createAndCompileEntry(const char* name, const BindingLayoutDesc& desc);

		BindingLayoutRef findEntry(const char* name) const;
		const BindingLayout& getEntry(BindingLayoutRef ref) const;
	};
}
