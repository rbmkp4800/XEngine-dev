#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>
#include <XLib.Time.h>

namespace XEngine::Render::Shaders::Builder
{
	class SourcesCache;

	using SourcesCacheEntryId = uint16;
	static constexpr SourcesCacheEntryId InvalidSourceCacheEntryId = SourcesCacheEntryId(-1);

	struct SourcesCacheEntry : public XLib::NonCopyable
	{
		friend SourcesCache;

	private:
		XLib::IntrusiveBinaryTreeNodeHook entriesSearchTreeHook;
		XLib::InplaceString<63, uint8> localPath;
		XLib::TimePoint writeTime = 0;
		const char* text = nullptr;
		bool writeTimeChecked = false;

	public:
		SourcesCacheEntry() = default;
		~SourcesCacheEntry() = default;

		XLib::TimePoint checkWriteTime(const char* rootPath);
		//const char* getLocalPath() const { return localPath.cstr(); }
		const char* loadText();
	};

	class SourcesCache : public XLib::NonCopyable
	{
	private:
		using EntriesSearchTree = XLib::IntrusiveBinaryTree<SourcesCacheEntry, &SourcesCacheEntry::entriesSearchTreeHook>;
		using EntriesStorageList = XLib::StaticSegmentedArrayList<SourcesCacheEntry, 5, 14>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;

	public:
		SourcesCache() = default;
		~SourcesCache() = default;

		SourcesCacheEntryId findOrCreateEntry(const char* localPath);
		SourcesCacheEntry& getEntry(SourcesCacheEntryId id);
	};
}
