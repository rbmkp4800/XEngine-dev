#pragma once

#include <XLib.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

namespace XEngine::Render::Shaders::Builder_
{
	class DescriptorSetLayoutList;

	class DescriptorSetLayout : public XLib::NonCopyable
	{
		friend DescriptorSetLayoutList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		XLib::StringViewASCII name;
		uint64 nameXSH = 0;

		const HAL::ShaderCompiler::DescriptorSetBindingDesc* bindings = nullptr;
		uint8 bindingCount = 0;

		HAL::ShaderCompiler::CompiledDescriptorSetLayout compiledDescriptorSetLayout;

	private:
		DescriptorSetLayout() = default;
		~DescriptorSetLayout() = default;

	public:
		bool compile();

		inline XLib::StringViewASCII getName() const { return name; }
		inline uint64 getNameXSH() const { return nameXSH; }
		inline const HAL::ShaderCompiler::CompiledDescriptorSetLayout& getCompiled() const { return compiledDescriptorSetLayout; }

		static inline ordering CompareOrdered(const DescriptorSetLayout& left, const DescriptorSetLayout& right) { return compare(left.nameXSH, right.nameXSH); }
	};

	enum class DescriptorSetLayoutCreationStatus
	{
		Success = 0,
		Failure_TooManyBindings,
		Failure_EntryNameDuplication,
		//Failure_EntryNameHashCollision,
	};

	struct DescriptorSetLayoutCreationResult
	{
		DescriptorSetLayoutCreationStatus status;
		DescriptorSetLayout* descriptorSetLayout;
	};

	class DescriptorSetLayoutList : public XLib::NonCopyable
	{
	private:
		struct EntrySearchTreeComparator abstract final
		{
			static inline ordering Compare(const DescriptorSetLayout& left, const DescriptorSetLayout& right) { return compare(left.nameXSH, right.nameXSH); }
			static inline ordering Compare(const DescriptorSetLayout& left, uint64 right) { return compare(left.nameXSH, right); }
		};

		using EntrySearchTree =
			XLib::IntrusiveBinaryTree<DescriptorSetLayout, &DescriptorSetLayout::searchTreeHook, EntrySearchTreeComparator>;

	private:
		EntrySearchTree entrySearchTree;
		uint32 entryCount = 0;

	public:
		using Iterator = EntrySearchTree::Iterator;

	public:
		DescriptorSetLayoutCreationResult create(XLib::StringViewASCII name,
			const HAL::ShaderCompiler::DescriptorSetBindingDesc* bindings, uint8 bindingCount);

		DescriptorSetLayout* find(XLib::StringViewASCII name) const;

		inline uint32 getSize() const { return entryCount; }

		inline Iterator begin() { return entrySearchTree.begin(); }
		inline Iterator end() { return entrySearchTree.end(); }
	};
}
