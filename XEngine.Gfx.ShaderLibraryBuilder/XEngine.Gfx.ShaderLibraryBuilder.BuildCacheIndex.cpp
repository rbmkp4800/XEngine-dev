#include <XLib.Allocation.h>
#include <XLib.CRC.h>
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
	if (header->outLibraryFilePathStringIndex >= header->stringTableSize)
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

	const BuildCacheIndexFormat::ShaderRecord* shaderTable =			(BuildCacheIndexFormat::ShaderRecord*)		(uintptr(fileData) + shaderTableOffset);
	const BuildCacheIndexFormat::SourceFileRecord* sourceFileTable =	(BuildCacheIndexFormat::SourceFileRecord*)	(uintptr(fileData) + sourceFileTableOffset);
	const uint16* shaderSourceFilesIndirectionTable =	(uint16*)	(uintptr(fileData) + shaderSourceFilesIndirectionTableOffset);
	const uint32* stringTable =							(uint32*)	(uintptr(fileData) + stringTableOffset);
	const char* stringPool =							(char*)		(uintptr(fileData) + stringPoolOffset);

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
}

void BuildCacheIndexWriter::buildAndStoreToFile(const char* pathCStr) const
{

}
