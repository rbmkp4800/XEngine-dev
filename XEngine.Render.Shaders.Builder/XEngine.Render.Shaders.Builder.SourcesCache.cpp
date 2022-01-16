#include <XLib.System.File.h>
#include <XLib.FileSystem.h>
#include <XLib.String.h>

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

	InplaceString1024 fullPath;
	fullPath.append(parentCache.getSourcesRootPath());
	fullPath.append(localPath);
	// TODO: Assert total length

	writeTime = FileSystem::GetFileLastWriteTime(fullPath.getCStr());
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

	InplaceString1024 fullPath;
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
	const uint32 fileSize = file.getSize();

	text.resize(fileSize + 1); // To have zero terminator in any case.
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

SourcesCacheEntry* SourcesCache::findOrCreateEntry(const char* localPath)
{
	EntriesSearchTree::Iterator existingIt = entriesSearchTree.find(localPath);
	if (existingIt)
		return existingIt;

	SourcesCacheEntry& entry = entriesStorageList.emplaceBack(*this);
	entry.localPath = localPath;

	entriesSearchTree.insert(entry);

	return &entry;
}
