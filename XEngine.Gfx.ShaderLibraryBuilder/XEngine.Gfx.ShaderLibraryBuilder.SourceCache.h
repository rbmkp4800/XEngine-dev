#pragma once

#include <XLib.h>
#include <XLib.Containers.BinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>
#include <XLib.Time.h>

namespace XEngine::Gfx::ShaderLibraryBuilder
{
	class SourceCache : public XLib::NonCopyable
	{
	private:
		struct Entry
		{
			XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;
			XLib::DynamicStringASCII text;
			XLib::StringViewASCII normalizedLocalPath; // Zero terminated. Stored after `Entry` itself.
			//XLib::TimePoint writeTime;
			bool textWasReadSuccessfully = false;
		};

		struct EntriesSearchTreeComparator abstract final
		{
			static inline ordering Compare(const Entry& left, const Entry& right);
			static inline ordering Compare(const Entry& left, const XLib::StringViewASCII& right);
		};

		using EntrySearchTree = XLib::IntrusiveBinaryTree<Entry, &Entry::searchTreeHook, EntriesSearchTreeComparator>;

	private:
		EntrySearchTree entrySearchTree;
		XLib::StringViewASCII rootPath;

	public:
		SourceCache(XLib::StringViewASCII rootPath) : rootPath(rootPath) {}
		~SourceCache() = default;

		bool resolveText(XLib::StringViewASCII localPath, XLib::StringViewASCII& resultText);
	};
}
