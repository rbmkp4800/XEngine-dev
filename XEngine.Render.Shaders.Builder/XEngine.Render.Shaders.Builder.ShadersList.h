#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include "XEngine.Render.Shaders.Builder.Common.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

namespace XEngine::Render::Shaders::Builder
{
	class ShadersList;

	class ShadersListEntry : public XLib::NonCopyable
	{
		friend ShadersList;

	private:
		using SourceDependenciesList = XLib::ExpandableInplaceArrayList<SourcesCacheEntryId, 14, uint16, false>;

	private:
		XLib::IntrusiveBinaryTreeNodeHook shadersSearchTreeHook;

		XLib::InplaceString<63, uint8> name;
		XLib::InplaceString<31, uint8> entryPointName;
		SourceDependenciesList sourceDependencies;

		SourcesCacheEntryId sourceMain = 0;
		ShaderType type = ShaderType::None;

		bool compilationRequired = false;

	public:
		ShadersListEntry() = default;
		~ShadersListEntry() = default;

		inline SourcesCacheEntryId getSourceMain() const { return sourceMain; }
		inline ShaderType getShaderType() const { return type; }

		inline void addSourceDependency(SourcesCacheEntryId id) { sourceDependencies.pushBack(id); }
		inline void clearSourceDependencies() { sourceDependencies.clear(); }
		inline uint16 getSourceDependencyCount() const { return sourceDependencies.getSize(); }
		//inline SourcesCacheEntryId getSourceDependency(uint16 i) const { return sourceDependencies[i]; }

		inline void setCompilationRequired() { compilationRequired = true; }
		inline bool isCompilationRequired() const { return compilationRequired; }
	};

	class ShadersList : public XLib::NonCopyable
	{
	private:
		using EntriesSearchTree = XLib::IntrusiveBinaryTree<ShadersListEntry, &ShadersListEntry::shadersSearchTreeHook>;
		using EntriesStorageList = XLib::StaticSegmentedArrayList<ShadersListEntry, 5, 16>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;

	public:
		using Iterator = EntriesSearchTree::Iterator;

	public:
		ShadersList() = default;
		~ShadersList() = default;

		// returns null if entry with this name already exists
		ShadersListEntry* createEntry(const char* name, ShaderType type, SourcesCacheEntryId mainSourceId);

		inline bool isEmpty() const { return entriesStorageList.isEmpty(); }
		inline uint32 getSize() const { return entriesStorageList.getSize(); }

		inline Iterator begin() { return entriesSearchTree.begin(); }
		inline Iterator end() { return entriesSearchTree.end(); }
	};
}
