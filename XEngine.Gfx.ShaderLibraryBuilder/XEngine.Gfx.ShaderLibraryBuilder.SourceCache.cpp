#include <XLib.FileSystem.h>
#include <XLib.Path.h>
#include <XLib.System.File.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.SourceCache.h"

using namespace XLib;
using namespace XEngine::Gfx::ShaderLibraryBuilder;

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
	text.growBufferToFitLength(fileSize);
	text.setLength(fileSize);

	const bool readResult = file.read(text.getData(), fileSize);

	file.close();

	if (!readResult)
		return false;

	resultText = AsRValue(text);
	return true;
}

inline ordering SourceCache::EntriesSearchTreeComparator::Compare(const Entry& left, const Entry& right)
{
	return CompareStringsOrderedCaseInsensitive(left.path, right.path);
}

inline ordering SourceCache::EntriesSearchTreeComparator::Compare(const Entry& left, const StringViewASCII& right)
{
	return CompareStringsOrderedCaseInsensitive(left.path, right);
}

bool SourceCache::resolve(StringViewASCII path, StringViewASCII& resultText)
{
	resultText = {};

	// TODO: Proper path validation and normalization.

	//if (!Path::IsValid(localPath)))
	//	return false;
	//if (!Path::IsRelative(localPath))
	//	return false;
	//if (!Path::HasFileName(localPath))
	//	return false;

	InplaceStringASCIIx1024 normalizedPath;
	Path::MakeAbsolute(path, normalizedPath);
	Path::Normalize(normalizedPath);

	if (Entry* existingEntry = entrySearchTree.find(normalizedPath.getView()))
	{
		if (existingEntry->textWasReadSuccessfully)
			resultText = existingEntry->text;
		return existingEntry->textWasReadSuccessfully;
	}

	/*InplaceStringASCIIx1024 fullPath;
	// TODO: Replace this shit with proper `Path::Combine`
	fullPath.append(rootPath);
	if (!fullPath.isEmpty())
		fullPath.append('/');
	fullPath.append(normalizedLocalPath);
	XAssert(!fullPath.isFull());*/

	DynamicStringASCII text;
	const bool readTextResult = ReadTextFile(normalizedPath.getCStr(), text);

	//if (!readTextResult)
	//	TextWriteFmtStdOut("Cannot open file '", fullPath, "'\n");

	const uintptr memoryBlockSize = sizeof(Entry) + normalizedPath.getLength() + 1;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize);

	memoryCopy((char*)memoryBlock + sizeof(Entry), normalizedPath.getData(), normalizedPath.getLength());

	Entry& newEntry = *(Entry*)memoryBlock;
	construct(newEntry);
	newEntry.text = AsRValue(text);
	newEntry.path = StringViewASCII((char*)memoryBlock + sizeof(Entry), normalizedPath.getLength());
	newEntry.textWasReadSuccessfully = readTextResult;
	entrySearchTree.insert(newEntry);

	resultText = newEntry.text;
	return readTextResult;
}
