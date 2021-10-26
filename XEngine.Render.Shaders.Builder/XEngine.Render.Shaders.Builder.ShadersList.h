#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

#include "XEngine.Render.Shaders.Builder.BindingLayoutsList.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

namespace XEngine::Render::Shaders::Builder_
{
	class ShadersList;

	using ShaderRef = uint16;

	class Shader : public XLib::NonCopyable
	{
		friend ShadersList;

	private:
		using EntryPointNameInplaceString = XLib::InplaceString<31, uint8>;
		using SourceDependenciesList = XLib::ExpandableInplaceArrayList<SourcesCacheEntryId, 14, uint16, false>;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		EntryPointNameInplaceString entryPointName;
		SourceDependenciesList sourceDependencies;

		HAL::ShaderCompiler::CompiledShader compilationResult;

		SourcesCacheEntryId sourceMain = ZeroSourcesCacheEntryId;
		BindingLayoutRef bindingLayout = ZeroBindingLayoutRef;
		HAL::ShaderCompiler::ShaderType type = HAL::ShaderCompiler::ShaderType::Undefined;

		bool compilationRequired = false;

	private:
		Shader() = default;
		~Shader() = default;

	public:
		inline SourcesCacheEntryId getSourceMain() const { return sourceMain; }
		inline BindingLayoutRef getBindingLayout() const { return bindingLayout; }
		inline HAL::ShaderCompiler::ShaderType getType() const { return type; }

		inline void addSourceDependency(SourcesCacheEntryId id) { sourceDependencies.pushBack(id); }
		inline void clearSourceDependencies() { sourceDependencies.clear(); }
		inline uint16 getSourceDependencyCount() const { return sourceDependencies.getSize(); }
		//inline SourcesCacheEntryId getSourceDependency(uint16 i) const { return sourceDependencies[i]; }

		inline void setCompilationRequired() { compilationRequired = true; }
		inline bool isCompilationRequired() const { return compilationRequired; }

		inline const HAL::ShaderCompiler::CompiledShader& getCompiled() const { return compilationResult; }

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
		Shader* createEntry(HAL::ShaderCompiler::ShaderType type, SourcesCacheEntryId mainSourceId);

		inline bool isEmpty() const { return entriesStorageList.isEmpty(); }
		inline uint32 getSize() const { return entriesStorageList.getSize(); }

		inline Iterator begin() { return entriesSearchTree.begin(); }
		inline Iterator end() { return entriesSearchTree.end(); }
	};
}
