#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

namespace XEngine::Render::Shaders::Builder_
{
	class PipelineLayoutsList;

	struct BindPointDesc
	{
		const char* name;
		HAL::PipelineBindPointType type;
		HAL::ShaderCompiler::PipelineBindPointShaderVisibility shaderVisibility;
		uint8 constantsSize32bitValues;
		// DescriptorTableRef
	};

	class PipelineLayout : public XLib::NonCopyable
	{
		friend PipelineLayoutsList;

	private:
		PipelineLayoutsList& parentList;

		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;
		const char* name = nullptr;
		uint64 nameCRC = 0;

		uint32 bindPointsOffsetInParentStorage = 0;
		uint8 bindPointCount = 0;

		HAL::ShaderCompiler::CompiledPipelineLayout compiledPipelineLayout;

	private:
		~PipelineLayout() = default;

	public:
		inline PipelineLayout(PipelineLayoutsList& parentList) : parentList(parentList) {}

		bool compile();

		inline const char* getName() const { return name; }
		inline uint64 getNameCRC() const { return nameCRC; }
		inline const HAL::ShaderCompiler::CompiledPipelineLayout& getCompiled() const { return compiledPipelineLayout; }
	};

	class PipelineLayoutsList : public XLib::NonCopyable
	{
		friend PipelineLayout;

	private:
		struct EntriesSearchTreeComparator;

		using EntriesSearchTree = XLib::IntrusiveBinaryTree<PipelineLayout, &PipelineLayout::searchTreeHook, EntriesSearchTreeComparator>;
		using EntriesStorageList = XLib::FixedLogSegmentedArrayList<PipelineLayout, 4, 10>;
		using BindPointsStorageList = XLib::ArrayList<BindPointDesc, uint32, false>;

	private:
		EntriesSearchTree entriesOrderedSearchTree;
		EntriesStorageList entriesStorageList;
		BindPointsStorageList bindPointsStorageList;

	public:
		using Iterator = EntriesSearchTree::Iterator;

	public:
		PipelineLayoutsList() = default;
		~PipelineLayoutsList() = default;

		// Returns null if entry with such name already exists.
		PipelineLayout* createEntry(const char* name, const BindPointDesc* bindPoints, uint8 bindPointCount);

		PipelineLayout* findEntry(const char* name) const;

		inline uint32 getSize() const { return entriesStorageList.getSize(); }

		inline Iterator begin() { return entriesOrderedSearchTree.begin(); }
		inline Iterator end() { return entriesOrderedSearchTree.end(); }
	};

	struct PipelineLayoutsList::EntriesSearchTreeComparator abstract final
	{
		static inline ordering Compare(const PipelineLayout& left, const PipelineLayout& right) { return compare(left.nameCRC, right.nameCRC); }
		static inline ordering Compare(const PipelineLayout& left, uint64 right) { return compare(left.nameCRC, right); }
	};
}
