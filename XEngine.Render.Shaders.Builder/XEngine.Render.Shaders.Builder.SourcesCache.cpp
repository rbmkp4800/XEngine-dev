#include <XLib.CRC.h>
#include <XLib.FileSystem.h>
#include <XLib.Path.h>
#include <XLib.String.h>
#include <XLib.System.File.h>
#include <XLib.Text.h>

#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder_;

static inline ordering CompareStringsOrderedCaseInsensitive(const StringView& left, const StringView& right)
{
	const uintptr leftLength = left.getLength();
	const uintptr rightLength = right.getLength();

	for (uintptr i = 0; i < leftLength; i++)
	{
		if (i >= rightLength)
			return ordering::greater;

		const char cLeft = ToUpper(left[i]);
		const char cRight = ToUpper(right[i]);
		if (cLeft > cRight)
			return ordering::greater;
		if (cLeft < cRight)
			return ordering::less;
	}

	return leftLength == rightLength ? ordering::equivalent : ordering::less;
}

static inline bool ValidateSourceFilePath(const StringView& path)
{
	if (!Path::IsRelative(path))
		return false;
	if (!Path::HasFileName(path))
		return false;
	
	for (char c : path)
	{
		if (!IsLetterOrDigit(c) && c != '_')
			return false;
	}

	return true;
}

// SourceFile //////////////////////////////////////////////////////////////////////////////////////

enum class SourceFile::TextState : uint8
{
	Unknown = 0,
	Loaded,
	CanNotOpen,
};

TimePoint SourceFile::checkWriteTime()
{
	if (writeTimeChecked)
		return writeTime;

	String fullPath; // TODO: Use `InplaceString1024` when ready.
	fullPath.append(parentCache.getRootPath());
	fullPath.append(localPath);
	// TODO: Assert total length

	// TODO:
	XAssert(false);
	writeTime = 0; // FileSystem::GetFileLastWriteTime(fullPath.getCStr());
	writeTimeChecked = true;

	if (writeTime == InvalidTimePoint)
		textState = TextState::CanNotOpen;

	return writeTime;
}

bool SourceFile::retrieveText(StringView& resultText)
{
	resultText = {};

	if (textState == TextState::CanNotOpen)
		return false;
	if (textState == TextState::Loaded)
	{
		resultText = text.getView();
		return true;
	}

	XAssert(text.isEmpty());

	InplaceString1024 fullPath;
	fullPath.append(parentCache.getRootPath());
	fullPath.append('/');
	fullPath.append(localPath);

	XAssert(!fullPath.isFull());

	File file;
	if (!file.open(fullPath.getCStr(), FileAccessMode::Read, FileOpenMode::OpenExisting))
	{
		textState = TextState::CanNotOpen;
		return false;
	}

	// TODO: Check for overflows
	const uint32 fileSize = uint32(file.getSize());

	text.resizeUnsafe(fileSize);
	const bool readResult = file.read(text.getMutableData(), fileSize);

	file.close();

	if (!readResult)
	{
		text.clear();
		text.compact();
		textState = TextState::CanNotOpen;
		return false;
	}

	textState = TextState::Loaded;
	resultText = text.getView();
	return true;
}

ordering SourceFile::CompareOrdered(const SourceFile& left, const SourceFile& right)
{
	return CompareStringsOrderedCaseInsensitive(left.localPath, right.localPath);
}

// SourcesCache ////////////////////////////////////////////////////////////////////////////////////

ordering SourcesCache::EntriesSearchTreeComparator::Compare(const SourceFile& left, const SourceFile& right)
{
	return CompareStringsOrderedCaseInsensitive(left.localPath, right.localPath);
}

ordering SourcesCache::EntriesSearchTreeComparator::Compare(const SourceFile& left, const StringView& right)
{
	return CompareStringsOrderedCaseInsensitive(left.localPath, right);
}

void SourcesCache::initialize(StringView rootPath)
{
	XAssert(!initialized);
	const bool pathStringOverflow = !this->rootPath.copyFrom(rootPath);
	XAssert(!pathStringOverflow);
	initialized = true;
}

SourcesCache::EntryCreationResult SourcesCache::createEntryIfAbsent(StringView localPath)
{
	XAssert(initialized);

	if (localPath.getLength() > SourceFile::InplaceLocalPathString::GetMaxLength())
		return EntryCreationResult { nullptr, EntryCreationStatus::Failure_PathIsTooLong };

	SourceFile::InplaceLocalPathString normalizedLocalPath;
	Path::Normalize(localPath, normalizedLocalPath);

	if (!ValidateSourceFilePath(normalizedLocalPath))
		return EntryCreationResult { nullptr, EntryCreationStatus::Failure_InvalidPath };

	const EntriesSearchTree::Iterator existingItemIt = entriesSearchTree.find(normalizedLocalPath);
	if (existingItemIt)
		return EntryCreationResult { existingItemIt, EntryCreationStatus::Success };

	SourceFile& entry = entriesStorageList.emplaceBack(*this);
	entry.localPath = normalizedLocalPath;

	entriesSearchTree.insert(entry);

	return EntryCreationResult { existingItemIt, EntryCreationStatus::Success };
}
