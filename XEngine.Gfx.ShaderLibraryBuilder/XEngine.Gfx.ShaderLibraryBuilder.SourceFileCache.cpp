#include <XLib.FileSystem.h>
#include <XLib.Fmt.h>
#include <XLib.Path.h>
#include <XLib.System.File.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.SourceFileCache.h"

using namespace XLib;
using namespace XEngine::Gfx::ShaderLibraryBuilder;

XTODO("Implement proper SourceFileHandle. For now it is just pointer to binary tree node");

enum class SourceFileCache::EntryTextState : uint8
{
	NotLoaded = 0,
	Loaded,
	LoadingFailed,
};

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

static bool ReadTextFile(const char* path, DynamicStringASCII& resultText)
{
	resultText = {};

	File file;
	if (!file.open(path, FileAccessMode::Read, FileOpenMode::OpenExisting))
		return false;

	const uint64 fileSize = file.getSize();
	if (fileSize == uint64(-1))
		return false;
	if (fileSize >= uint64(uint32(-1)))
		return false;
	const uint32 fileSizeU32 = uint32(fileSize);

	DynamicStringASCII text;
	text.growBufferToFitLength(fileSizeU32);
	text.setLength(fileSizeU32);
	if (!file.read(text.getData(), fileSizeU32))
		return false;

	resultText = AsRValue(text);
	return true;
}

inline ordering SourceFileCache::EntriesSearchTreeComparator::Compare(const Entry& left, const Entry& right)
{
	return CompareStringsOrderedCaseInsensitive(left.path, right.path);
}

inline ordering SourceFileCache::EntriesSearchTreeComparator::Compare(const Entry& left, const StringViewASCII& right)
{
	return CompareStringsOrderedCaseInsensitive(left.path, right);
}

SourceFileHandle SourceFileCache::openFile(StringViewASCII path)
{
	XAssert(Path::IsAbsolute(path));
	if (!Path::HasFileName(path))
		return SourceFileHandle(0);

	InplaceStringASCIIx1024 normalizedPath;
	Path::Normalize(path, normalizedPath);

	// TODO: Do propper uppercasing.
	for (uint16 i = 0; i < normalizedPath.getLength(); i++)
		normalizedPath[i] = Char::ToUpper(normalizedPath[i]);

	if (Entry* existingEntry = entrySearchTree.find(normalizedPath.getView()))
		return SourceFileHandle(uint64(existingEntry));

	const uint64 modTime = FileSystem::GetFileModificationTime(normalizedPath.getCStr());
	if (modTime == InvalidTimePoint)
		return SourceFileHandle(0);

	const uintptr memoryBlockSize = sizeof(Entry) + normalizedPath.getLength() + 1;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize);

	memoryCopy((char*)memoryBlock + sizeof(Entry), normalizedPath.getData(), normalizedPath.getLength());

	Entry& newEntry = *(Entry*)memoryBlock;
	XConstruct(newEntry);
	newEntry.path = StringViewASCII((char*)memoryBlock + sizeof(Entry), normalizedPath.getLength());
	newEntry.modTime = modTime;
	newEntry.textState = EntryTextState::NotLoaded;

	entrySearchTree.insert(newEntry);

	return SourceFileHandle(uint64(memoryBlock));
}

StringViewASCII SourceFileCache::getFilePath(SourceFileHandle fileHandle) const
{
	XAssert(fileHandle != SourceFileHandle(0));
	Entry* entry = (Entry*)uint64(fileHandle);
	return entry->path;
}

uint64 SourceFileCache::getFileModTime(SourceFileHandle fileHandle) const
{
	XAssert(fileHandle != SourceFileHandle(0));
	Entry* entry = (Entry*)uint64(fileHandle);
	return entry->modTime;
}

bool SourceFileCache::getFileText(SourceFileHandle fileHandle, XLib::StringViewASCII& resultText)
{
	Entry* entry = (Entry*)uint64(fileHandle);

	if (entry->textState == EntryTextState::NotLoaded)
	{
		const bool readTextResult = ReadTextFile(entry->path.getData(), entry->text);
		if (!readTextResult)
			FmtPrintStdOut("error: failed to read contents of a file '", entry->path, "' (but the file seems to exist)\n");	

		entry->textState = readTextResult ? EntryTextState::Loaded : EntryTextState::LoadingFailed;
	}

	resultText = entry->text;
	return entry->textState == EntryTextState::Loaded;
}
