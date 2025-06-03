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

	const uint32 fileSize = XCheckedCastU32(file.getSize());

	DynamicStringASCII text;
	text.growBufferToFitLength(fileSize);
	text.setLength(fileSize);

	const bool readResult = file.read(text.getData(), fileSize);
	file.close();

	if (!readResult)
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
	InplaceStringASCIIx1024 normalizedPath;
	Path::MakeAbsolute(path, normalizedPath);

	// TODO: Do propper uppercasing.
	for (uint16 i = 0; i < normalizedPath.getLength(); i++)
		normalizedPath[i] = Char::ToUpper(normalizedPath[i]);

	if (Entry* existingEntry = entrySearchTree.find(normalizedPath.getView()))
		return SourceFileHandle(uint64(existingEntry));

	const FileSystemOpResult<TimePoint> getModTimeResult = FileSystem::GetFileModificationTime(normalizedPath.getCStr());
	if (getModTimeResult.status != FileSystemOpStatus::Success)
		return SourceFileHandle(0);

	const uintptr memoryBlockSize = sizeof(Entry) + normalizedPath.getLength() + 1;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize);

	memoryCopy((char*)memoryBlock + sizeof(Entry), normalizedPath.getData(), normalizedPath.getLength());

	Entry& newEntry = *(Entry*)memoryBlock;
	construct(newEntry);
	newEntry.path = StringViewASCII((char*)memoryBlock + sizeof(Entry), normalizedPath.getLength());
	newEntry.modTime = getModTimeResult.value;
	newEntry.textState = EntryTextState::NotLoaded;

	entrySearchTree.insert(newEntry);

	return SourceFileHandle(uint64(memoryBlock));
}

StringViewASCII SourceFileCache::getFilePath(SourceFileHandle fileHandle) const
{
	Entry* entry = (Entry*)uint64(fileHandle);
	return entry->path;
}

uint64 SourceFileCache::getFileModTime(SourceFileHandle fileHandle) const
{
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
