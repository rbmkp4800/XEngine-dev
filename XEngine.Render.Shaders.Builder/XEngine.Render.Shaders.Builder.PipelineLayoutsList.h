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

	struct BindPointDesc
	{
		XLib::StringView name;
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
		XLib::InplaceString64 name;
		uint64 nameCRC = 0;

		uint32 bindPointsOffsetInParentStorage = 0;
		uint8 bindPointCount = 0;

		HAL::ShaderCompiler::CompiledPipelineLayout compiledPipelineLayout;

	private:
		~PipelineLayout() = default;

	public:
		inline PipelineLayout(PipelineLayoutsList& parentList) : parentList(parentList) {}

		bool compile();

		inline XLib::StringView getName() const { return name; }
		inline uint64 getNameCRC() const { return nameCRC; }
		inline const HAL::ShaderCompiler::CompiledPipelineLayout& getCompiled() const { return compiledPipelineLayout; }

		static inline ordering CompareOrdered(const PipelineLayout& left, const PipelineLayout& right) { return compare(left.nameCRC, right.nameCRC); }
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
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;
		BindPointsStorageList bindPointsStorageList;

	public:
		using Iterator = EntriesSearchTree::Iterator;

	public:
		PipelineLayoutsList() = default;
		~PipelineLayoutsList() = default;

		// Returns null if entry with such name already exists.
		PipelineLayout* createEntry(XLib::StringView name, const BindPointDesc* bindPoints, uint8 bindPointCount);

		PipelineLayout* findEntry(XLib::StringView name) const;

		inline uint32 getSize() const { return entriesStorageList.getSize(); }

		inline Iterator begin() { return entriesSearchTree.begin(); }
		inline Iterator end() { return entriesSearchTree.end(); }
	};

	struct PipelineLayoutsList::EntriesSearchTreeComparator abstract final
	{
		static inline ordering Compare(const PipelineLayout& left, const PipelineLayout& right) { return compare(left.nameCRC, right.nameCRC); }
		static inline ordering Compare(const PipelineLayout& left, uint64 right) { return compare(left.nameCRC, right); }
	};
}
