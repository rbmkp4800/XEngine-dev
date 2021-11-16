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
		struct BindPoint
		{
			XLib::InplaceString<31, uint8> name;
			HAL::PipelineBindPointType type;
			HAL::ShaderCompiler::PipelineBindPointShaderVisibility shaderVisibility;
			uint8 constantsSize32bitValues;
		};

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		XLib::InplaceString<63, uint8> name;
		XLib::ArrayList<BindPoint, uint8> bindPoints;

		HAL::ShaderCompiler::CompiledPipelineLayout compiledPipelineLayout;

	private:
		PipelineLayout() = default;
		~PipelineLayout() = default;

	public:
		bool compile();

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
		using Iterator = EntriesSearchTree::Iterator;

	public:
		PipelineLayoutsList() = default;
		~PipelineLayoutsList() = default;

		PipelineLayoutRef createEntry(const char* name, const HAL::ShaderCompiler::PipelineLayoutDesc& desc);

		PipelineLayoutRef findEntry(const char* name) const;
		const PipelineLayout& getEntry(PipelineLayoutRef ref) const;

		inline Iterator begin() { return entriesSearchTree.begin(); }
		inline Iterator end() { return entriesSearchTree.end(); }
	};
}
