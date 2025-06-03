#include <algorithm>
#include <unordered_map>

#include <XLib.Allocation.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.CRC.h>
#include <XLib.FileSystem.h>
#include <XLib.String.h>
#include <XLib.System.File.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.BuildCacheIndex.h"

using namespace XLib;
using namespace XEngine::Gfx::ShaderLibraryBuilder;

// BuildCacheIndexReader ///////////////////////////////////////////////////////////////////////////

StringViewASCII BuildCacheIndexReader::getString(uint32 stringIndex) const
{
	XAssert(stringIndex < header->stringTableSize);
	const uint32 stringOffset = stringTable[stringIndex];
	const uint32 nextStringOffset = (stringIndex + 1 < header->stringTableSize) ? stringTable[stringIndex + 1] : header->stringPoolSize;
	XAssert(stringOffset < nextStringOffset);
	XAssert(!stringPool[nextStringOffset - 1]);
	const uint32 stringLength = nextStringOffset - stringOffset - 1;
	return StringViewASCII(stringPool + stringOffset, stringLength);
}

BuildCacheIndexReader::~BuildCacheIndexReader()
{
	if (fileData)
	{
		SystemHeapAllocator::Release((void*)fileData);
		memorySet(this, 0, sizeof(this));
	}
}

bool BuildCacheIndexReader::loadFromFile(const char* pathCStr, uint32 expectedMagic)
{
	XTODO(__FUNCTION__ ": Release memory on failure");

	File file;
	file.open(pathCStr, FileAccessMode::Read, FileOpenMode::OpenExisting);
	if (!file.isOpen())
		return false;

	const uint64 fileSize = file.getSize();
	void* fileData = SystemHeapAllocator::Allocate(fileSize);
	if (!file.read(fileData, fileSize))
		return false;
	file.close();

	const BuildCacheIndexFormat::Header* header = (BuildCacheIndexFormat::Header*)fileData;
	if (header->magic != expectedMagic)
		return false;
	if (header->fileSize != fileSize)
		return false;
	if (!header->shaderCount || header->shaderCount == uint16(-1))
		return false;
	if (!header->sourceFileCount)
		return false;
	if (!header->stringTableSize)
		return false;

	if (header->manifestFilePathStringIndex >= header->stringTableSize)
		return false;
	if (header->libraryFilePathStringIndex >= header->stringTableSize && header->libraryFilePathStringIndex != uint32(-1))
		return false;

	uint32 offsetAccum = 0;
	offsetAccum += sizeof(BuildCacheIndexFormat::Header);
	offsetAccum = alignUp<uint32>(offsetAccum, BuildCacheIndexFormat::SectionsAlignment);

	const uint32 shaderTableOffset = offsetAccum;
	offsetAccum += sizeof(BuildCacheIndexFormat::ShaderRecord) * header->shaderCount;
	offsetAccum = alignUp<uint32>(offsetAccum, BuildCacheIndexFormat::SectionsAlignment);

	const uint32 sourceFileTableOffset = offsetAccum;
	offsetAccum += sizeof(BuildCacheIndexFormat::SourceFileRecord) * header->sourceFileCount;
	offsetAccum = alignUp<uint32>(offsetAccum, BuildCacheIndexFormat::SectionsAlignment);

	const uint32 shaderSourceFilesIndirectionTableOffset = offsetAccum;
	offsetAccum += sizeof(uint16) * header->shaderSourceFilesIndirectionTableSize;
	offsetAccum = alignUp<uint32>(offsetAccum, BuildCacheIndexFormat::SectionsAlignment);

	const uint32 stringTableOffset = offsetAccum;
	offsetAccum += sizeof(uint32) * header->stringTableSize;
	offsetAccum = alignUp<uint32>(offsetAccum, BuildCacheIndexFormat::SectionsAlignment);

	const uint32 stringPoolOffset = offsetAccum;
	offsetAccum += header->stringPoolSize;

	const uint32 fileSizeCheck = offsetAccum;
	if (fileSizeCheck != fileSize)
		return false;

	const BuildCacheIndexFormat::ShaderRecord* shaderTable = (BuildCacheIndexFormat::ShaderRecord*)(uintptr(fileData) + shaderTableOffset);
	const BuildCacheIndexFormat::SourceFileRecord* sourceFileTable = (BuildCacheIndexFormat::SourceFileRecord*)(uintptr(fileData) + sourceFileTableOffset);
	const uint16* shaderSourceFilesIndirectionTable = (uint16*)(uintptr(fileData) + shaderSourceFilesIndirectionTableOffset);
	const uint32* stringTable = (uint32*)(uintptr(fileData) + stringTableOffset);
	const char* stringPool = (char*)(uintptr(fileData) + stringPoolOffset);

	// Validate data.

	uint64 prevShaderNameXSH = 0;
	uint32 prevShaderIndirectionTableSegmentEnd = 0;
	for (uint16 i = 0; i < header->shaderCount; i++)
	{
		const BuildCacheIndexFormat::ShaderRecord& shader = shaderTable[i];
		if (shader.nameXSH <= prevShaderNameXSH)
			return false;
		if (!shader.sourceFileCount)
			return false;
		if (shader.sourceFilesIndirectionTableOffset + shader.sourceFileCount > header->shaderSourceFilesIndirectionTableSize)
			return false;
		if (shader.sourceFilesIndirectionTableOffset != prevShaderIndirectionTableSegmentEnd)
			return false;
		if (shader.type == HAL::ShaderType::Undefined || shader.type >= HAL::ShaderType::ValueCount)
			return false;

		prevShaderNameXSH = shader.nameXSH;
		prevShaderIndirectionTableSegmentEnd = shader.sourceFilesIndirectionTableOffset + shader.sourceFileCount;
	}

	for (uint16 i = 0; i < header->sourceFileCount; i++)
	{
		const BuildCacheIndexFormat::SourceFileRecord& sourceFile = sourceFileTable[i];
		if (sourceFile.pathStringIndex >= header->stringTableSize)
			return false;
	}

	for (uint32 i = 0; i < header->shaderSourceFilesIndirectionTableSize; i++)
	{
		if (shaderSourceFilesIndirectionTable[i] >= header->sourceFileCount)
			return false;
	}

	if (stringTable[0] != 0)
		return false;
	for (uint32 i = 0; i < header->stringTableSize; i++)
	{
		const uint32 stringOffset = stringTable[i];
		if (stringOffset + 1 >= header->stringPoolSize)
			return false;

		const uint32 nextStringOffset = (i + 1 < header->stringTableSize) ? stringTable[i + 1] : header->stringPoolSize;
		if (stringOffset >= nextStringOffset)
			return false;
		if (stringPool[nextStringOffset - 1] != 0)
			return false;
	}

	// File seems to be valid.

	this->fileData = fileData;
	this->header = header;
	this->shaderTable = shaderTable;
	this->sourceFileTable = sourceFileTable;
	this->shaderSourceFilesIndirectionTable = shaderSourceFilesIndirectionTable;
	this->stringTable = stringTable;
	this->stringPool = stringPool;

	return true;
}

uint16 BuildCacheIndexReader::getShaderSourceFileCount(uint16 shaderIndex) const
{
	XAssert(shaderIndex < header->shaderCount);
	return shaderTable[shaderIndex].sourceFileCount;
}

uint16 BuildCacheIndexReader::getShaderSourceFileGlobalIndex(uint16 shaderIndex, uint16 shaderSourceFileIndex) const
{
	XAssert(shaderIndex < header->shaderCount);
	const BuildCacheIndexFormat::ShaderRecord& shader = shaderTable[shaderIndex];
	XAssert(shaderSourceFileIndex < shader.sourceFileCount);
	const uint32 indirectionIndex = shader.sourceFilesIndirectionTableOffset + shaderSourceFileIndex;
	XAssert(indirectionIndex < header->shaderSourceFilesIndirectionTableSize);
	return shaderSourceFilesIndirectionTable[indirectionIndex];
}

StringViewASCII BuildCacheIndexReader::getSourceFilePath(uint16 globalSourceFileIndex) const
{
	XAssert(globalSourceFileIndex < header->sourceFileCount);
	return getString(sourceFileTable[globalSourceFileIndex].pathStringIndex);
}

uint64 BuildCacheIndexReader::getSourceFileModTime(uint16 globalSourceFileIndex) const
{
	XAssert(globalSourceFileIndex < header->sourceFileCount);
	const BuildCacheIndexFormat::SourceFileRecord& sourceFile = sourceFileTable[globalSourceFileIndex];
	return (uint64(sourceFile.modTime1) << 32) | uint64(sourceFile.modTime0);
}

uint16 BuildCacheIndexReader::findShader(uint64 shaderNameXSH) const
{
	XTODO(__FUNCTION__ ": Use binary search here");
	for (uint16 i = 0; i < header->shaderCount; i++)
	{
		if (shaderTable[i].nameXSH == shaderNameXSH)
			return i;
	}
	return uint16(-1);
}

bool BuildCacheIndexReader::doShaderDetailsMatch(uint16 shaderIndex,
	uint64 pipelineLayoutNameXSH, uint64 pipelineLayoutHash,
	const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs) const
{
	XAssert(shaderIndex < header->shaderCount);
	const BuildCacheIndexFormat::ShaderRecord& shader = shaderTable[shaderIndex];

	if (shader.pipelineLayoutNameXSH != uint32(pipelineLayoutNameXSH))
		return false;
	if (shader.pipelineLayoutHash != uint32(pipelineLayoutHash))
		return false;
	const uint32 entryPointNameHash = CRC32::Compute(compilationArgs.entryPointName.getData(), compilationArgs.entryPointName.getLength());
	if (shader.entryPointNameHash != entryPointNameHash)
		return false;
	if (shader.definesHash != 0)
		return false;
	if (shader.type != compilationArgs.shaderType)
		return false;

	return true;
}


// BuildCacheIndexWriter ///////////////////////////////////////////////////////////////////////////

void BuildCacheIndexWriter::addShader(uint64 nameXSH,
	uint64 pipelineLayoutNameXSH, uint64 pipelineLayoutHash,
	const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs,
	const SourceFileHandle* sourceFiles, uint16 sourceFileCount)
{
	XAssert(sourceFileCount > 0);

	Shader& shader = shaders.emplaceBack();
	shader.nameXSH = nameXSH;
	shader.pipelineLayoutNameXSH = uint32(pipelineLayoutNameXSH);
	shader.pipelineLayoutHash = uint32(pipelineLayoutHash);
	shader.entryPointNameHash = CRC32::Compute(compilationArgs.entryPointName.getData(), compilationArgs.entryPointName.getLength());
	shader.definesHash = 0;
	shader.globalSourceFilesStoreOffset = globalShaderSourceFilesStore.getSize();
	shader.sourceFileCount = sourceFileCount;
	shader.type = compilationArgs.shaderType;

	for (uint16 i = 0; i < sourceFileCount; i++)
	{
		XAssert(sourceFiles[i] != SourceFileHandle(0));
		globalShaderSourceFilesStore.pushBack(sourceFiles[i]);
	}
}

void BuildCacheIndexWriter::buildAndStoreToFile(const char* pathCStr, uint32 magic, const SourceFileCache& sourceFileCache,
	const char* manifestFilePathCStr, const char* libraryFilePathCStr)
{
	const uint16 shaderCount = XCheckedCastU16(shaders.getSize());

	auto shadersSortPredicate = [](const Shader& left, const Shader& right) -> bool { return left.nameXSH < right.nameXSH; };
	std::sort(shaders.begin(), shaders.end(), shadersSortPredicate);

	XTODO(__FUNCTION__ ": Replace 'std::unordered_map'");
	std::unordered_map<SourceFileHandle, uint16> sourceFileToSortedIndexMap;

	for (SourceFileHandle sourceFile : globalShaderSourceFilesStore)
		sourceFileToSortedIndexMap.insert({ sourceFile, 0 });

	ArrayList<SourceFileHandle> sourceFileList;
	for (const std::pair<SourceFileHandle, uint16>& mapItem : sourceFileToSortedIndexMap)
		sourceFileList.pushBack(mapItem.first);

	auto sourceFilesSortPredicate = [&sourceFileCache](SourceFileHandle left, SourceFileHandle right) -> bool
		{ return String::IsLess(sourceFileCache.getFilePath(left), sourceFileCache.getFilePath(right)); };
	std::sort(sourceFileList.begin(), sourceFileList.end(), sourceFilesSortPredicate);

	const uint16 sourceFileCount = XCheckedCastU16(sourceFileList.getSize());
	for (uint16 i = 0; i < sourceFileCount; i++)
		sourceFileToSortedIndexMap[sourceFileList[i]] = i;

	uint32 stringPoolSizeAccum = 0;
	ArrayList<StringViewASCII> strings;

	const uint32 manifestFilePathStringIndex = strings.getSize();
	const StringViewASCII manifestFilePath = StringViewASCII::FromCStr(manifestFilePathCStr);
	strings.pushBack(manifestFilePath);
	stringPoolSizeAccum += uint32(manifestFilePath.getLength() + 1);

	uint32 libraryFilePathStringIndex = uint32(-1);
	if (libraryFilePathCStr && *libraryFilePathCStr)
	{
		libraryFilePathStringIndex = strings.getSize();
		const StringViewASCII libraryFilePath = StringViewASCII::FromCStr(libraryFilePathCStr);
		strings.pushBack(libraryFilePath);
		stringPoolSizeAccum += uint32(libraryFilePath.getLength() + 1);
	}

	ArrayList<uint32> sourceFilePathStringIndices;
	sourceFilePathStringIndices.resize(sourceFileCount);
	for (uint16 i = 0; i < sourceFileCount; i++)
	{
		const StringViewASCII sourceFilePath = sourceFileCache.getFilePath(sourceFileList[i]);
		sourceFilePathStringIndices[i] = strings.getSize();
		strings.pushBack(sourceFilePath);
		stringPoolSizeAccum += uint32(sourceFilePath.getLength() + 1);
	}

	const uint32 stringPoolSize = stringPoolSizeAccum;

	uint32 offsetAccum = 0;
	offsetAccum += sizeof(BuildCacheIndexFormat::Header);
	offsetAccum = alignUp<uint32>(offsetAccum, BuildCacheIndexFormat::SectionsAlignment);

	const uint32 shaderTableOffset = offsetAccum;
	offsetAccum += sizeof(BuildCacheIndexFormat::ShaderRecord) * shaderCount;
	offsetAccum = alignUp<uint32>(offsetAccum, BuildCacheIndexFormat::SectionsAlignment);

	const uint32 sourceFileTableOffset = offsetAccum;
	offsetAccum += sizeof(BuildCacheIndexFormat::SourceFileRecord) * sourceFileCount;
	offsetAccum = alignUp<uint32>(offsetAccum, BuildCacheIndexFormat::SectionsAlignment);

	const uint32 shaderSourceFilesIndirectionTableOffset = offsetAccum;
	offsetAccum += sizeof(uint16) * globalShaderSourceFilesStore.getSize();
	offsetAccum = alignUp<uint32>(offsetAccum, BuildCacheIndexFormat::SectionsAlignment);

	const uint32 stringTableOffset = offsetAccum;
	offsetAccum += sizeof(uint32) * strings.getSize();
	offsetAccum = alignUp<uint32>(offsetAccum, BuildCacheIndexFormat::SectionsAlignment);

	const uint32 stringPoolOffset = offsetAccum;
	offsetAccum += stringPoolSize;

	const uint32 fileSize = offsetAccum;

	void* fileData = SystemHeapAllocator::Allocate(fileSize);
	memorySet(fileData, 0, fileSize);

	BuildCacheIndexFormat::Header* header = (BuildCacheIndexFormat::Header*)fileData;
	BuildCacheIndexFormat::ShaderRecord* shaderTable = (BuildCacheIndexFormat::ShaderRecord*)(uintptr(fileData) + shaderTableOffset);
	BuildCacheIndexFormat::SourceFileRecord* sourceFileTable = (BuildCacheIndexFormat::SourceFileRecord*)(uintptr(fileData) + sourceFileTableOffset);
	uint16* shaderSourceFilesIndirectionTable = (uint16*)(uintptr(fileData) + shaderSourceFilesIndirectionTableOffset);
	uint32* stringTable = (uint32*)(uintptr(fileData) + stringTableOffset);
	char* stringPool = (char*)(uintptr(fileData) + stringPoolOffset);

	header->magic = magic;
	header->fileSize = fileSize;
	header->shaderCount = shaderCount;
	header->sourceFileCount = sourceFileCount;
	header->shaderSourceFilesIndirectionTableSize = globalShaderSourceFilesStore.getSize();
	header->stringTableSize = strings.getSize();
	header->stringPoolSize = stringPoolSize;

	header->manifestFilePathStringIndex = manifestFilePathStringIndex;
	header->libraryFilePathStringIndex = libraryFilePathStringIndex;
	header->manifestFileModTime = FileSystem::GetFileModificationTime(manifestFilePathCStr).value;
	header->libraryFileModTime = libraryFilePathStringIndex == uint32(-1) ?
		InvalidTimePoint : FileSystem::GetFileModificationTime(libraryFilePathCStr).value;

	uint32 shaderSourceFilesIndirectionTableOffsetAccum = 0;
	for (uint16 shaderIndex = 0; shaderIndex < shaderCount; shaderIndex++)
	{
		const Shader& srcShader = shaders[shaderIndex];
		BuildCacheIndexFormat::ShaderRecord& shaderRecord = shaderTable[shaderIndex];

		shaderRecord.nameXSH = srcShader.nameXSH;
		shaderRecord.pipelineLayoutNameXSH = srcShader.pipelineLayoutNameXSH;
		shaderRecord.pipelineLayoutHash = srcShader.pipelineLayoutHash;
		shaderRecord.entryPointNameHash = srcShader.entryPointNameHash;
		shaderRecord.definesHash = srcShader.definesHash;
		shaderRecord.sourceFilesIndirectionTableOffset = shaderSourceFilesIndirectionTableOffsetAccum;
		shaderRecord.sourceFileCount = srcShader.sourceFileCount;
		shaderRecord.type = srcShader.type;

		for (uint16 i = 0; i < srcShader.sourceFileCount; i++)
		{
			const SourceFileHandle sourceFile = globalShaderSourceFilesStore[srcShader.globalSourceFilesStoreOffset + i];
			const uint16 sourceFileIndex = sourceFileToSortedIndexMap[sourceFile];

			XAssert(shaderSourceFilesIndirectionTableOffsetAccum < globalShaderSourceFilesStore.getSize());
			shaderSourceFilesIndirectionTable[shaderSourceFilesIndirectionTableOffsetAccum] = sourceFileIndex;
			shaderSourceFilesIndirectionTableOffsetAccum++;
		}
	}
	XAssert(shaderSourceFilesIndirectionTableOffsetAccum == globalShaderSourceFilesStore.getSize());

	for (uint16 i = 0; i < sourceFileCount; i++)
	{
		const uint64 sourceFileModTime = sourceFileCache.getFileModTime(sourceFileList[i]);
		BuildCacheIndexFormat::SourceFileRecord& sourceFileRecord = sourceFileTable[i];

		sourceFileRecord.pathStringIndex = sourceFilePathStringIndices[i];
		sourceFileRecord.modTime0 = uint32(sourceFileModTime);
		sourceFileRecord.modTime1 = uint32(sourceFileModTime >> 32);
	}

	uint32 stringPoolOffsetAccum = 0;
	for (uint32 i = 0; i < strings.getSize(); i++)
	{
		stringTable[i] = stringPoolOffsetAccum;

		const StringViewASCII string = strings[i];
		XAssert(stringPoolOffsetAccum + string.getLength() < stringPoolSize);
		memoryCopy(stringPool + stringPoolOffsetAccum, string.getData(), string.getLength());
		stringPoolOffsetAccum += uint32(string.getLength() + 1);
	}
	XAssert(stringPoolOffsetAccum == stringPoolSize);

	File file;
	file.open(pathCStr, FileAccessMode::Write, FileOpenMode::Override);
	if (file.isOpen())
	{
		file.write(fileData, fileSize);
		file.close();
	}

	SystemHeapAllocator::Release(fileData);
}
