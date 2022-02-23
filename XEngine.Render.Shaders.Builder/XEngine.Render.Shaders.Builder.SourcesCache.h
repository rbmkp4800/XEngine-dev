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

	struct SourceFile : public XLib::NonCopyable
	{
		friend SourcesCache;

	private:
		enum class TextState : uint8;
		using InplaceLocalPathString = XLib::InplaceString128;

	private:
		SourcesCache& parentCache;

		XLib::IntrusiveBinaryTreeNodeHook entriesSearchTreeHook;
		InplaceLocalPathString localPath;

		XLib::String text;
		TextState textState = TextState(0);

		XLib::TimePoint writeTime = 0;
		bool writeTimeChecked = false;

	private:
		~SourceFile() = default;

	public:
		inline SourceFile(SourcesCache& parentCache) : parentCache(parentCache) {}

		// Returns `InvalidTimePoint` if can't open file.
		XLib::TimePoint checkWriteTime();

		// Returns false if can't open file.
		bool retrieveText(XLib::StringView& resultText);

		inline XLib::StringView getLocalPath() const { return localPath; }
		inline const char* getLocalPathCStr() const { return localPath.getCStr(); }

		static ordering CompareOrdered(const SourceFile& left, const SourceFile& right);
	};

	class SourcesCache : public XLib::NonCopyable
	{
	private:
		struct EntriesSearchTreeComparator abstract final
		{
			static ordering Compare(const SourceFile& left, const SourceFile& right);
			static ordering Compare(const SourceFile& left, const XLib::StringView& right);
		};

		using EntriesSearchTree = XLib::IntrusiveBinaryTree<SourceFile, &SourceFile::entriesSearchTreeHook, EntriesSearchTreeComparator>;
		using EntriesStorageList = XLib::FixedLogSegmentedArrayList<SourceFile, 5, 16>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;
		XLib::InplaceString512 rootPath;
		bool initialized = false;

	public:
		enum class EntryCreationStatus
		{
			Success = 0,
			Failure_PathIsTooLong,
			Failure_InvalidPath,
		};

		struct EntryCreationResult
		{
			SourceFile* entry;
			EntryCreationStatus status;
		};

	public:
		SourcesCache() = default;
		~SourcesCache() = default;

		void initialize(XLib::StringView rootPath);

		EntryCreationResult createEntryIfAbsent(XLib::StringView localPath);

		inline XLib::StringView getRootPath() const { return rootPath; }
	};
}
