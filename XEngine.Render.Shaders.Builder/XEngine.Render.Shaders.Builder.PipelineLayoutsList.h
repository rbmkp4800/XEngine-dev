#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

namespace XEngine::Render::Shaders::Builder_
{
	class PipelineLayoutsList;

	enum class PipelineLayoutRef : uint16;
	static constexpr PipelineLayoutRef ZeroPipelineLayoutRef = PipelineLayoutRef(0);

	class PipelineLayout : public XLib::NonCopyable
	{
		friend PipelineLayoutsList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		XLib::InplaceString<63, uint8> name;
		HAL::ShaderCompiler::CompiledPipelineLayout compiledPipelineLayout;

	private:
		PipelineLayout() = default;
		~PipelineLayout() = default;

	public:
		inline const HAL::ShaderCompiler::CompiledPipelineLayout& getCompiled() const { return compiledPipelineLayout; }
	};

	class PipelineLayoutsList : public XLib::NonCopyable
	{
	private:
		using EntriesSearchTree = XLib::IntrusiveBinaryTree<PipelineLayout, &PipelineLayout::searchTreeHook>;
		using EntriesStorageList = XLib::StaticSegmentedArrayList<PipelineLayout, 4, 10>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;

	public:
		PipelineLayoutsList() = default;
		~PipelineLayoutsList() = default;

		PipelineLayoutRef createEntry(const char* name, const HAL::ShaderCompiler::PipelineLayoutDesc& desc);

		PipelineLayoutRef findEntry(const char* name) const;
		const PipelineLayout& getEntry(PipelineLayoutRef ref) const;
	};
}
