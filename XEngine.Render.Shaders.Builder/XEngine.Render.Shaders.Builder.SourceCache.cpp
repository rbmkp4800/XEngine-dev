#include <XLib.CRC.h>
#include <XLib.FileSystem.h>
#include <XLib.Path.h>
#include <XLib.String.h>
#include <XLib.System.File.h>
#include <XLib.Text.h>

#include "XEngine.Render.Shaders.Builder.SourceCache.h"

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

static inline bool ValidateSourcePath(const StringView& path)
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

// Source //////////////////////////////////////////////////////////////////////////////////////

enum class Source::TextState : uint8
{
	Unknown = 0,
	Loaded,
	CanNotOpen,
};

TimePoint Source::checkWriteTime()
{
	if (writeTimeChecked)
		return writeTime;

	InplaceString1024 fullPath;
	fullPath.append(parentCache->getRootPath());
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

bool Source::retrieveText(StringView& resultText)
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
	fullPath.append(parentCache->getRootPath());
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
	const bool readResult = file.read(text.getData(), fileSize);

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

ordering Source::CompareOrdered(const Source& left, const Source& right)
{
	return CompareStringsOrderedCaseInsensitive(left.localPath, right.localPath);
}

// SourceCache ////////////////////////////////////////////////////////////////////////////////////

ordering SourceCache::EntriesSearchTreeComparator::Compare(const Source& left, const Source& right)
{
	return CompareStringsOrderedCaseInsensitive(left.localPath, right.localPath);
}

ordering SourceCache::EntriesSearchTreeComparator::Compare(const Source& left, const StringView& right)
{
	return CompareStringsOrderedCaseInsensitive(left.localPath, right);
}

void SourceCache::initialize(StringView rootPath)
{
	XAssert(!initialized);
	const bool pathStringOverflow = !this->rootPath.copyFrom(rootPath);
	XAssert(!pathStringOverflow);
	initialized = true;
}

SourceCreationResult SourceCache::findOrCreate(StringView localPath)
{
	XAssert(initialized);

	// TODO: Check if path is too long.

	InplaceString1024 normalizedLocalPath;
	Path::Normalize(localPath, normalizedLocalPath);

	if (!ValidateSourcePath(normalizedLocalPath))
		return SourceCreationResult { SourceCreationStatus::Failure_InvalidPath };

	const EntrySearchTree::Iterator existingItemIt = entrySearchTree.find(normalizedLocalPath);
	if (existingItemIt)
		return SourceCreationResult { SourceCreationStatus::Success, existingItemIt };

	const uint32 memoryBlockSize = sizeof(Source) + uint32(normalizedLocalPath.getLength()) + 1;
	byte* sourceMemory = (byte*)SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(sourceMemory, 0, memoryBlockSize);

	char* localPathMemory = (char*)(sourceMemory + sizeof(Source));
	memoryCopy(localPathMemory, normalizedLocalPath.getData(), normalizedLocalPath.getLength());
	localPathMemory[normalizedLocalPath.getLength()] = '\0';

	Source& source = *(Source*)sourceMemory;
	source.parentCache = this;
	source.localPath = StringView(localPathMemory, normalizedLocalPath.getLength());

	entrySearchTree.insert(source);

	return SourceCreationResult { SourceCreationStatus::Success, &source };
}
