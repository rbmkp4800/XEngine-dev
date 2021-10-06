#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

#include "XEngine.Render.Shaders.Builder.BindingLayoutsList.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

namespace XEngine::Render::Shaders::Builder
{
	class ShadersList;

	using ShaderRef = uint16;

	class Shader : public XLib::NonCopyable
	{
		friend ShadersList;

	private:
		using NameInplaceString = XLib::InplaceString<63, uint8>;
		using EntryPointNameInplaceString = XLib::InplaceString<31, uint8>;
		using SourceDependenciesList = XLib::ExpandableInplaceArrayList<SourcesCacheEntryId, 14, uint16, false>;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		NameInplaceString name;
		EntryPointNameInplaceString entryPointName;
		SourceDependenciesList sourceDependencies;

		SourcesCacheEntryId sourceMain = 0;
		BindingLayoutRef bindingLayout = BindingLayoutRef(0);
		HAL::ShaderCompiler::ShaderType type = HAL::ShaderCompiler::ShaderType::None;

		bool compilationRequired = false;

	private:
		Shader() = default;
		~Shader() = default;

	public:
		inline SourcesCacheEntryId getSourceMain() const { return sourceMain; }
		inline HAL::ShaderCompiler::ShaderType getShaderType() const { return type; }

		inline void addSourceDependency(SourcesCacheEntryId id) { sourceDependencies.pushBack(id); }
		inline void clearSourceDependencies() { sourceDependencies.clear(); }
		inline uint16 getSourceDependencyCount() const { return sourceDependencies.getSize(); }
		//inline SourcesCacheEntryId getSourceDependency(uint16 i) const { return sourceDependencies[i]; }

		inline void setCompilationRequired() { compilationRequired = true; }
		inline bool isCompilationRequired() const { return compilationRequired; }

		static constexpr uintptr NameLengthLimit = NameInplaceString::GetMaxLength();
		static constexpr uintptr EntryPointNameLengthLimit = EntryPointNameInplaceString::GetMaxLength();
	};

	class ShadersList : public XLib::NonCopyable
	{
	private:
		using EntriesSearchTree = XLib::IntrusiveBinaryTree<Shader, &Shader::searchTreeHook>;
		using EntriesStorageList = XLib::StaticSegmentedArrayList<Shader, 5, 16>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;

	public:
		using Iterator = EntriesSearchTree::Iterator;

	public:
		ShadersList() = default;
		~ShadersList() = default;

		// returns null if entry with this name already exists
		Shader* createEntry(StringView name, HAL::ShaderCompiler::ShaderType type, SourcesCacheEntryId mainSourceId);

		inline bool isEmpty() const { return entriesStorageList.isEmpty(); }
		inline uint32 getSize() const { return entriesStorageList.getSize(); }

		inline Iterator begin() { return entriesSearchTree.begin(); }
		inline Iterator end() { return entriesSearchTree.end(); }
	};
}
