#include <XLib.CRC.h>
#include <XLib.FileSystem.h>
#include <XLib.String.h>
#include <XLib.System.File.h>

#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder_;

enum class SourcesCacheEntry::TextState : uint8
{
	Unknown = 0,
	Loaded,
	CanNotOpen,
};

TimePoint SourcesCacheEntry::checkWriteTime()
{
	if (writeTimeChecked)
		return writeTime;

	String fullPath; // TODO: Use `InplaceString1024` when ready.
	fullPath.append(parentCache.getSourcesRootPath());
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

bool SourcesCacheEntry::retrieveText(StringView& resultText)
{
	resultText = {};

	if (textState == TextState::CanNotOpen)
		return false;
	if (textState == TextState::Loaded)
	{
		resultText = text.getView();
		return true;
	}

	String fullPath; // TODO: Use `InplaceString1024` when ready.
	fullPath.append(parentCache.getSourcesRootPath());
	fullPath.append(localPath);

	XAssert(text.isEmpty());

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

SourceFile* SourcesCache::findOrCreateEntry(StringView localPath)
{
	XAssert(sourcesRootPath);

	if (localPath.isEmpty())
		return nullptr;

	// TODO: Normalize path.
	// TODO: Use lowercase for CRC computation.
	const uint64 localPathCRC = CRC64::Compute(localPath.getData(), localPath.getLength());

	const EntriesSearchTree::Iterator existingItemIt = entriesSearchTree.find(localPathCRC);
	if (existingItemIt)
		return existingItemIt;

	SourceFile& entry = entriesStorageList.emplaceBack(*this);
	entry.localPath = localPath;
	entry.localPathCRC = localPathCRC;

	entriesSearchTree.insert(entry);

	return entry;
}
