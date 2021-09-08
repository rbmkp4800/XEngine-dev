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
		using LocalPathInplaceString = XLib::InplaceString<63, uint8>;

	private:
		XLib::IntrusiveBinaryTreeNodeHook entriesSearchTreeHook;
		LocalPathInplaceString localPath;
		XLib::TimePoint writeTime = 0;
		const char* text = nullptr;
		bool writeTimeChecked = false;

	private:
		SourcesCacheEntry() = default;
		~SourcesCacheEntry() = default;

	public:
		XLib::TimePoint checkWriteTime(const char* rootPath);
		//const char* getLocalPath() const { return localPath.cstr(); }
		StringView loadText();

		static constexpr uintptr LocalPathLengthLimit = LocalPathInplaceString::GetMaxLength();
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

		SourcesCacheEntryId findOrCreateEntry(StringView localPath);
		SourcesCacheEntry& getEntry(SourcesCacheEntryId id);
	};
}
