#include <XLib.FileSystem.h>
#include <XLib.Path.h>
#include <XLib.String.h>
#include <XLib.System.File.h>
#include <XLib.Text.h>

#include "XEngine.Render.Shaders.Builder.SourceCache.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder_;

static inline ordering CompareStringsOrderedCaseInsensitive(const StringViewASCII& left, const StringViewASCII& right)
{
	const uintptr leftLength = left.getLength();
	const uintptr rightLength = right.getLength();

	for (uintptr i = 0; i < leftLength; i++)
	{
		if (i >= rightLength)
			return ordering::greater;

		const char cLeft = Char::ToUpper(left[i]);
		const char cRight = Char::ToUpper(right[i]);
		if (cLeft > cRight)
			return ordering::greater;
		if (cLeft < cRight)
			return ordering::less;
	}

	return leftLength == rightLength ? ordering::equivalent : ordering::less;
}

static inline bool ValidateSourcePath(const StringViewASCII& path)
{
	//if (!Path::IsRelative(path.getData()))
	//	return false;
	//if (!Path::HasFileName(path))
	//	return false;
	
	for (char c : path)
	{
		if (!Char::IsLetterOrDigit(c) && c != '_' && c != '-' && c != '+' && c != '.' && c != '/')
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

	XAssert(false);

	InplaceStringASCIIx1024 fullPath;
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

bool Source::retrieveText(StringViewASCII& resultText)
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

	InplaceStringASCIIx1024 fullPath;
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

ordering SourceCache::EntriesSearchTreeComparator::Compare(const Source& left, const StringViewASCII& right)
{
	return CompareStringsOrderedCaseInsensitive(left.localPath, right);
}

void SourceCache::initialize(StringViewASCII rootPath)
{
	XAssert(!initialized);
	const bool pathStringOverflow = !this->rootPath.copyFrom(rootPath);
	XAssert(!pathStringOverflow);
	initialized = true;
}

SourceCreationResult SourceCache::findOrCreate(StringViewASCII localPath)
{
	XAssert(initialized);

	// TODO: Check if path is too long.

	InplaceStringASCIIx1024 normalizedLocalPath;
	// TODO:
//	Path::Normalize(localPath, normalizedLocalPath);
	normalizedLocalPath.append(localPath);

	if (!ValidateSourcePath(normalizedLocalPath))
		return SourceCreationResult { SourceCreationStatus::Failure_InvalidPath };

	if (Source* existingSource = entrySearchTree.find(normalizedLocalPath))
		return SourceCreationResult { SourceCreationStatus::Success, existingSource };

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
