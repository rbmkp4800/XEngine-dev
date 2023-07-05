#include <XLib.FileSystem.h>
#include <XLib.Path.h>
#include <XLib.System.File.h>

#include "XEngine.Render.ShaderLibraryBuilder.SourceCache.h"

using namespace XLib;
using namespace XEngine::Render::ShaderLibraryBuilder;

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
	File file;
	if (!file.open(path, FileAccessMode::Read, FileOpenMode::OpenExisting))
		return false;

	const uint32 fileSize = XCheckedCastU32(file.getSize());

	DynamicStringASCII text;
	text.resizeUnsafe(fileSize);

	const bool readResult = file.read(text.getData(), fileSize);

	file.close();

	if (!readResult)
		return false;

	resultText = asRValue(text);
	return true;
}

inline ordering SourceCache::EntriesSearchTreeComparator::Compare(const Entry& left, const Entry& right)
{
	return CompareStringsOrderedCaseInsensitive(left.normalizedLocalPath, right.normalizedLocalPath);
}

inline ordering SourceCache::EntriesSearchTreeComparator::Compare(const Entry& left, const StringViewASCII& right)
{
	return CompareStringsOrderedCaseInsensitive(left.normalizedLocalPath, right);
}

bool SourceCache::resolveText(XLib::StringViewASCII localPath, XLib::StringViewASCII& resultText)
{
	resultText = nullptr;

	XTODO("Proper path validation and normalization");
	XTODO("Proper error reporting")

	//if (!Path::IsValid(localPath)))
	//	return false;
	//if (!Path::IsRelative(localPath))
	//	return false;
	//if (!Path::HasFileName(localPath))
	//	return false;

	for (char c : localPath)
	{
		if (!Char::IsLetterOrDigit(c) && c != '_' && c != '-' && c != '+' && c != '.' && c != '/')
			return false;
	}

	InplaceStringASCIIx1024 normalizedLocalPath;
	//Path::Normalize(localPath, normalizedLocalPath);
	normalizedLocalPath = localPath;

	if (Entry* existingEntry = entrySearchTree.find(StringViewASCII(normalizedLocalPath)))
	{
		if (existingEntry->textWasReadSuccessfully)
			resultText = existingEntry->text;
		return existingEntry->textWasReadSuccessfully;
	}

	InplaceStringASCIIx1024 fullPath;
	fullPath.append(rootPath);
	fullPath.append('/');
	fullPath.append(normalizedLocalPath);
	XAssert(!fullPath.isFull());

	DynamicStringASCII text;
	const bool readTextResult = ReadTextFile(fullPath.getCStr(), text);

	const uint32 memoryBlockSize = sizeof(Entry) + uint32(normalizedLocalPath.getLength()) + 1;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);

	Entry& newEntry = *(Entry*)memoryBlock;
	char* newEntryNormalizedLocalPath = (char*)((Entry*)memoryBlock + 1);

	memoryCopy(newEntryNormalizedLocalPath, normalizedLocalPath.getData(), normalizedLocalPath.getLength());
	newEntryNormalizedLocalPath[normalizedLocalPath.getLength()] = 0;

	construct(newEntry);
	newEntry.text = asRValue(text);
	newEntry.normalizedLocalPath = StringViewASCII(newEntryNormalizedLocalPath, normalizedLocalPath.getLength());
	newEntry.textWasReadSuccessfully = readTextResult;
	entrySearchTree.insert(newEntry);

	resultText = newEntry.text;
	return readTextResult;
}
