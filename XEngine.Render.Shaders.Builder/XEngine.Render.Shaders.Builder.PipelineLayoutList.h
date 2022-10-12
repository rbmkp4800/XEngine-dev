#pragma once

#include <XLib.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

namespace XEngine::Render::Shaders::Builder_
{
	class PipelineLayoutList;

	class PipelineLayout : public XLib::NonCopyable
	{
		friend PipelineLayoutList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		XLib::StringViewASCII name;
		uint64 nameXSH = 0;

		HAL::ShaderCompiler::PipelineBindPointDesc* bindPoints = nullptr;
		uint8 bindPointCount = 0;

		HAL::ShaderCompiler::CompiledPipelineLayout compiledPipelineLayout;

	private:
		PipelineLayout() = default;
		~PipelineLayout() = default;

	public:
		bool compile();

		inline XLib::StringViewASCII getName() const { return name; }
		inline uint64 getNameXSH() const { return nameXSH; }
		inline const HAL::ShaderCompiler::CompiledPipelineLayout& getCompiled() const { return compiledPipelineLayout; }

		static inline ordering CompareOrdered(const PipelineLayout& left, const PipelineLayout& right) { return compare(left.nameXSH, right.nameXSH); }
	};

	enum class PipelineLayoutCreationStatus
	{
		Success = 0,
		Failure_TooManyBindPoints,
		Failure_EntryNameDuplication,
		//Failure_EntryNameHashCollision,
	};

	struct PipelineLayoutCreationResult
	{
		PipelineLayoutCreationStatus status;
		PipelineLayout* pipelineLayout;
	};

	class PipelineLayoutList : public XLib::NonCopyable
	{
		friend PipelineLayout;

	private:
		struct EntrySearchTreeComparator abstract final
		{
			static inline ordering Compare(const PipelineLayout& left, const PipelineLayout& right) { return compare(left.nameXSH, right.nameXSH); }
			static inline ordering Compare(const PipelineLayout& left, uint64 right) { return compare(left.nameXSH, right); }
		};

		using EntrySearchTree =
			XLib::IntrusiveBinaryTree<PipelineLayout, &PipelineLayout::searchTreeHook, EntrySearchTreeComparator>;

	private:
		EntrySearchTree entrySearchTree;
		uint32 entryCount = 0;

	public:
		using Iterator = EntrySearchTree::Iterator;

	public:
		PipelineLayoutList() = default;
		~PipelineLayoutList() = default;

		PipelineLayoutCreationResult create(XLib::StringViewASCII name,
			const HAL::ShaderCompiler::PipelineBindPointDesc* bindPoints, uint8 bindPointCount);

		PipelineLayout* find(XLib::StringViewASCII name) const;

		inline uint32 getSize() const { return entryCount; }

		inline Iterator begin() { return entrySearchTree.begin(); }
		inline Iterator end() { return entrySearchTree.end(); }
	};
}
