#pragma once

#include <XLib.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>
#include <XLib.Time.h>

namespace XEngine::Render::Shaders::Builder_
{
	class SourceCache;

	struct Source : public XLib::NonCopyable
	{
		friend SourceCache;

	private:
		enum class TextState : uint8;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		const SourceCache* parentCache = nullptr;
		XLib::StringViewASCII localPath; // Zero terminated.
		XLib::DynamicStringASCII text;
		TextState textState = TextState(0);

		XLib::TimePoint writeTime = 0;
		bool writeTimeChecked = false;

	private:
		Source() = default;
		~Source() = default;

	public:
		// Returns `InvalidTimePoint` if can't open file.
		XLib::TimePoint checkWriteTime();

		// Returns false if can't open file.
		bool retrieveText(XLib::StringViewASCII& resultText);

		inline XLib::StringViewASCII getLocalPath() const { return localPath; }
		inline const char* getLocalPathCStr() const { return localPath.getData(); }

		static ordering CompareOrdered(const Source& left, const Source& right);
	};

	enum class SourceCreationStatus
	{
		Success = 0,
		Failure_PathIsTooLong,
		Failure_InvalidPath,
	};

	struct SourceCreationResult
	{
		SourceCreationStatus status;
		Source* source;
	};

	class SourceCache : public XLib::NonCopyable
	{
	private:
		struct EntriesSearchTreeComparator abstract final
		{
			static ordering Compare(const Source& left, const Source& right);
			static ordering Compare(const Source& left, const XLib::StringViewASCII& right);
		};

		using EntrySearchTree = XLib::IntrusiveBinaryTree<Source, &Source::searchTreeHook, EntriesSearchTreeComparator>;

	private:
		EntrySearchTree entrySearchTree;
		XLib::InplaceStringASCIIx512 rootPath;
		bool initialized = false;

	public:
		SourceCache() = default;
		~SourceCache() = default;

		void initialize(XLib::StringViewASCII rootPath);

		SourceCreationResult findOrCreate(XLib::StringViewASCII localPath);

		inline XLib::StringViewASCII getRootPath() const { return rootPath; }
	};
}
