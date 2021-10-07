#include <XLib.FileSystem.h>
#include <XLib.String.h>

#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

// TODO:
#define ASSERT(...)

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder_;

XLib::TimePoint SourcesCacheEntry::checkWriteTime(const char* rootPath)
{
	if (writeTimeChecked)
		return writeTime;

	InplaceString1024 fullPath;
	fullPath.copyFrom(rootPath);
	fullPath.append(localPath.getView());
	// TODO: Assert total length

	writeTime = XLib::FileSystem::GetFileLastWriteTime(fullPath.getCStr());
	writeTimeChecked = true;

	return writeTime;
}

SourcesCacheEntryId SourcesCache::findOrCreateEntry(StringView localPath)
{
	EntriesSearchTree::Iterator it = entriesSearchTree.find(localPath);
	if (it.isValid())
	{
		const SourcesCacheEntry& entry = it.get();
		const uintptr index = entriesStorageList.calculateIndex(&entry);
		ASSERT(index < entriesStorageList.getSize());
		return SourcesCacheEntryId(index);
	}
	else
	{
		const uintptr index = entriesStorageList.getSize();
		SourcesCacheEntry& entry = entriesStorageList.emplaceBack();
		entry.localPath.copyFrom(localPath); // TODO: `fillFrom` and check length
		entriesSearchTree.insert(entry);
		return SourcesCacheEntryId(index);
	}
}

SourcesCacheEntry& SourcesCache::getEntry(const SourcesCacheEntryId id)
{
	ASSERT(id < entriesStorageList.getSize());
	return entriesStorageList[id];
}
