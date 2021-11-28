#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>
#include <XLib.Time.h>

namespace XEngine::Render::Shaders::Builder_
{
	class SourcesCache;

	struct SourcesCacheEntry : public XLib::NonCopyable
	{
		friend SourcesCache;

	private:
		enum class TextState : uint8;

	private:
		SourcesCache& parentCache;

		XLib::IntrusiveBinaryTreeNodeHook entriesSearchTreeHook;
		const char* localPath = nullptr;

		XLib::String text;
		TextState textState = TextState(0);

		XLib::TimePoint writeTime = 0;
		bool writeTimeChecked = false;

	private:
		inline SourcesCacheEntry(SourcesCache& parentCache) : parentCache(parentCache) {}
		~SourcesCacheEntry() = default;

	public:
		// Returns `InvalidTimePoint` if can't open file.
		XLib::TimePoint checkWriteTime();

		// Returns false if can't open file.
		bool retrieveText(XLib::StringView& resultText);
	};

	class SourcesCache : public XLib::NonCopyable
	{
	private:
		using EntriesSearchTree = XLib::IntrusiveBinaryTree<SourcesCacheEntry, &SourcesCacheEntry::entriesSearchTreeHook>;
		using EntriesStorageList = XLib::FixedLogSegmentedArrayList<SourcesCacheEntry, 5, 16>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;
		const char* sourcesRootPath = nullptr;

	public:
		SourcesCache() = default;
		~SourcesCache() = default;

		void initialize(const char* sourcesRootPath);

		SourcesCacheEntry* findOrCreateEntry(const char* localPath);

		inline const char* getSourcesRootPath() const { return sourcesRootPath; }
	};
}
