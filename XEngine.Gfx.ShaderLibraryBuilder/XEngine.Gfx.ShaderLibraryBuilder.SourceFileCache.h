#pragma once

#include <XLib.h>
#include <XLib.Containers.BinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

namespace XEngine::Gfx::ShaderLibraryBuilder
{
	enum class SourceFileHandle : uint64 {};

	class SourceFileCache : public XLib::NonCopyable
	{
	private:
		enum class EntryTextState : uint8;

		struct Entry
		{
			XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;
			XLib::DynamicStringASCII text;
			XLib::StringViewASCII path; // Zero terminated. Stored after `Entry` itself.
			uint64 modTime;
			EntryTextState textState = EntryTextState(0);
		};

		struct EntriesSearchTreeComparator abstract final
		{
			static inline ordering Compare(const Entry& left, const Entry& right);
			static inline ordering Compare(const Entry& left, const XLib::StringViewASCII& right);
		};

		using EntrySearchTree = XLib::IntrusiveBinaryTree<Entry, &Entry::searchTreeHook, EntriesSearchTreeComparator>;

	private:
		EntrySearchTree entrySearchTree;

	public:
		SourceFileCache() = default;
		~SourceFileCache() = default;

		SourceFileHandle openFile(XLib::StringViewASCII path);

		bool getFileText(SourceFileHandle fileHandle, XLib::StringViewASCII& resultText);
		uint64 getFileModTime(SourceFileHandle fileHandle) const;
	};
}
