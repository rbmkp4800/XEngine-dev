#include <XLib.Allocation.h>
#include <XLib.CRC.h>
#include <XLib.System.Timer.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.BuildDepsTrace.h"

using namespace XLib;
using namespace XEngine::Gfx::ShaderLibraryBuilder::BuildDepsTrace;

static inline uint64 U64From2xU32(uint32 lo, uint32 hi) { return uint64(lo) | (uint64(hi) << 32); }

// BuildDepsTrace::Reader //////////////////////////////////////////////////////////////////////////

Reader::~Reader()
{
	if (fileData)
		SystemHeapAllocator::Release((void*)fileData);

	fileData = nullptr;
	manifestFileRecord = nullptr;
	builtLibraryFileRecord = nullptr;
}

bool Reader::load(const char* pathCStr, uint32 expectedMagic)
{
	XTODO(__FUNCTION__ ": Release memory on failure");

	XAssert(!fileData);

	File file;
	file.open(pathCStr, FileAccessMode::Read, FileOpenMode::OpenExisting);
	if (!file.isOpen())
		return false;

	const uint64 fileSize = file.getSize();
	if (fileSize == uint64(-1))
		return false;
	byte* fileData = (byte*) SystemHeapAllocator::Allocate(fileSize);
	if (!file.read(fileData, fileSize))
		return false;
	file.close();

	if (fileSize < sizeof(Format::Header))
		return false;

	const Format::Header* header = (Format::Header*)fileData;
	if (header->magic != expectedMagic)
		return false;

	struct
	{
		const Format::ManifestFileRecordBody* manifestFileRecord = nullptr;
		const Format::BuiltLibraryFileRecordBody* builtLibraryFileRecord = nullptr;
		ArrayList<const Format::SourceFileRecordBody*> sourceFiles;
		ArrayList<Shader> shaders;
	} tmp;

	uint32 recordOffset = sizeof(Format::Header);
	uint32 recordIndex = 0;

	while (recordOffset < fileSize)
	{
		XAssert(recordOffset % Format::RecordAlignment == 0);

		const byte* recordData = fileData + recordOffset;
		const Format::RecordHeader* recordHeader = (Format::RecordHeader*)recordData;

		if (recordOffset + sizeof(Format::RecordHeader) > fileSize)
			break;
		if (recordOffset + recordHeader->size > fileSize)
			break;
		if (recordHeader->size % Format::RecordAlignment != 0)
			break;
		if (recordHeader->size > Format::MaxRecordSize)
			break;

		const void* recordBody = recordData + sizeof(Format::RecordHeader);
		const void* recordEnd = recordData + recordHeader->size;

		uint16 recordHashCheck = 0;
		{
			Format::RecordHeader recordHeaderCopyWithZeroHash = *recordHeader;
			recordHeaderCopyWithZeroHash.hash = 0;

			CRC16 crc;
			crc.process(&recordHeaderCopyWithZeroHash, sizeof(Format::RecordHeader));
			crc.process(recordHeader + 1, recordHeader->size - sizeof(Format::RecordHeader));
			recordHashCheck = crc.getValue();
		}
		if (recordHeader->hash != recordHashCheck)
			break;

		// After this point, record is considered valid. Any subsequent errors indicate that overall file state is invalid.

		if (recordIndex == 0 && recordHeader->type != Format::RecordType::ManifestFile)
			return false;
		if (recordIndex > 0 && recordHeader->type == Format::RecordType::ManifestFile)
			return false;

		if (recordHeader->type == Format::RecordType::ManifestFile)
		{
			const Format::ManifestFileRecordBody* manifestFileRecordBody = (Format::ManifestFileRecordBody*)recordBody;
			const char* manifestFilePathString = (char*)(manifestFileRecordBody + 1);
			if (manifestFilePathString > recordEnd)
				return false;

			const byte* manifestFilePathStringEnd = (byte*)(manifestFilePathString + manifestFileRecordBody->pathLength + 1);
			if (manifestFilePathStringEnd > recordEnd)
				return false;
			if (manifestFilePathStringEnd + Format::RecordAlignment <= recordEnd)
				return false;
			if (manifestFilePathString[manifestFileRecordBody->pathLength] != 0)
				return false;

			tmp.manifestFileRecord = manifestFileRecordBody;
		}
		else if (recordHeader->type == Format::RecordType::SourceFile)
		{
			const Format::SourceFileRecordBody* sourceFileRecordBody = (Format::SourceFileRecordBody*)recordBody;
			const char* sourceFilePathString = (char*)(sourceFileRecordBody + 1);
			if (sourceFilePathString > recordEnd)
				return false;

			const byte* sourceFilePathStringEnd = (byte*)(sourceFilePathString + sourceFileRecordBody->pathLength + 1);
			if (sourceFilePathStringEnd > recordEnd)
				return false;
			if (sourceFilePathStringEnd + Format::RecordAlignment <= recordEnd)
				return false;
			if (sourceFilePathString[sourceFileRecordBody->pathLength] != 0)
				return false;

			if (tmp.sourceFiles.getSize() >= uint16(-1))
				return false;

			const uint16 expectedSourceFileIndex = uint16(tmp.sourceFiles.getSize());
			if (sourceFileRecordBody->index != expectedSourceFileIndex)
				return false;

			tmp.sourceFiles.pushBack(sourceFileRecordBody);
		}
		else if (recordHeader->type == Format::RecordType::Shader)
		{
			const Format::ShaderRecordBody* shaderRecordBody = (Format::ShaderRecordBody*)recordBody;
			const uint16* shaderSourceFileIndices = (uint16*)(shaderRecordBody + 1);
			if (shaderSourceFileIndices > recordEnd)
				return false;

			const byte* shaderSourceFileIndicesEnd = (byte*)(shaderSourceFileIndices + shaderRecordBody->sourceFileCount);
			if (shaderSourceFileIndicesEnd > recordEnd)
				return false;
			if (shaderSourceFileIndicesEnd + Format::RecordAlignment <= recordEnd)
				return false;

			if (tmp.shaders.getSize() >= uint16(-1))
				return false;

			for (uint16 i = 0; i < shaderRecordBody->sourceFileCount; i++)
			{
				if (shaderSourceFileIndices[i] >= tmp.sourceFiles.getSize())
					return false;
			}

			const uint64 nameXSH = U64From2xU32(shaderRecordBody->nameXSH0, shaderRecordBody->nameXSH1);
			tmp.shaders.pushBack(Shader { .nameXSH = nameXSH, .record = shaderRecordBody });
		}
		else if (recordHeader->type == Format::RecordType::BuiltLibraryFile)
		{
			const Format::BuiltLibraryFileRecordBody* builtLibraryFileRecordBody = (Format::BuiltLibraryFileRecordBody*)recordBody;
			const char* builtLibraryFilePathString = (char*)(builtLibraryFileRecordBody + 1);
			if (builtLibraryFilePathString > recordEnd)
				return false;

			const byte* builtLibraryFilePathStringEnd = (byte*)(builtLibraryFilePathString + builtLibraryFileRecordBody->pathLength + 1);
			if (builtLibraryFilePathStringEnd > recordEnd)
				return false;
			if (builtLibraryFilePathStringEnd + Format::RecordAlignment <= recordEnd)
				return false;
			if (builtLibraryFilePathString[builtLibraryFileRecordBody->pathLength] != 0)
				return false;

			tmp.builtLibraryFileRecord = builtLibraryFileRecordBody;

			if (recordOffset + recordHeader->size != fileSize)
				return false;
			break;
		}
		else
			return false;

		recordOffset += recordHeader->size;
		recordIndex++;
	}

	this->fileData = fileData;
	this->manifestFileRecord = tmp.manifestFileRecord;
	this->builtLibraryFileRecord = tmp.builtLibraryFileRecord;
	this->sourceFiles = AsRValue(tmp.sourceFiles);
	this->shaders = AsRValue(tmp.shaders);
	return true;
}

StringViewASCII Reader::getManifestFilePath() const
{
	XAssert(manifestFileRecord);
	const char* path = (char*)(manifestFileRecord + 1);
	return StringViewASCII(path, manifestFileRecord->pathLength);
}

uint64 Reader::getManifestFileModTime() const
{
	XAssert(manifestFileRecord);
	return U64From2xU32(manifestFileRecord->modTime0, manifestFileRecord->modTime1);
}

StringViewASCII Reader::getBuiltLibraryFilePath() const
{
	XAssert(builtLibraryFileRecord);
	const char* path = (char*)(builtLibraryFileRecord + 1);
	return StringViewASCII(path, builtLibraryFileRecord->pathLength);
}

uint64 Reader::getBuiltLibraryFileModTime() const
{
	XAssert(builtLibraryFileRecord);
	return U64From2xU32(builtLibraryFileRecord->modTime0, builtLibraryFileRecord->modTime1);
}

StringViewASCII Reader::getSourceFilePath(uint16 sourceFileIndex) const
{
	XAssert(sourceFileIndex < sourceFiles.getSize());
	const char* path = (char*)(sourceFiles[sourceFileIndex] + 1);
	return StringViewASCII(path, sourceFiles[sourceFileIndex]->pathLength);
}

uint64 Reader::getSourceFileModTime(uint16 sourceFileIndex) const
{
	XAssert(sourceFileIndex < sourceFiles.getSize());
	return U64From2xU32(sourceFiles[sourceFileIndex]->modTime0, sourceFiles[sourceFileIndex]->modTime1);
}

uint16 Reader::getShaderSourceFileCount(uint16 shaderIndex) const
{
	XAssert(shaderIndex < shaders.getSize());
	return shaders[shaderIndex].record->sourceFileCount;
}

uint16 Reader::getShaderSourceFileIndex(uint16 shaderIndex, uint16 localSourceFileIndex) const
{
	XAssert(shaderIndex < shaders.getSize());
	const Format::ShaderRecordBody* shaderRecord = shaders[shaderIndex].record;
	const uint16* shaderSourceFileIndices = (uint16*)(shaderRecord + 1);
	XAssert(localSourceFileIndex < shaderRecord->sourceFileCount);
	return shaderSourceFileIndices[localSourceFileIndex];
}

uint64 Reader::getShaderCompiledBlobModTime(uint16 shaderIndex) const
{
	XAssert(shaderIndex < shaders.getSize());
	const Format::ShaderRecordBody& shaderRecord = *shaders[shaderIndex].record;
	return U64From2xU32(shaderRecord.compiledBlobFileModTime0, shaderRecord.compiledBlobFileModTime1);
}

uint16 Reader::findShader(uint64 shaderNameXSH) const
{
	XTODO(__FUNCTION__ ": Sort shaders and use binary search here");
	for (uint16 i = 0; i < shaders.getSize(); i++)
	{
		if (shaders[i].nameXSH == shaderNameXSH)
			return i;
	}
	return uint16(-1);
}

bool Reader::doShaderDetailsMatch(uint16 shaderIndex,
	uint64 pipelineLayoutNameXSH, uint64 pipelineLayoutHash,
	const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs) const
{
	XAssert(shaderIndex < shaders.getSize());
	const Format::ShaderRecordBody& shaderRecord = *shaders[shaderIndex].record;

	if (shaderRecord.pipelineLayoutNameXSH != uint32(pipelineLayoutNameXSH))
		return false;
	if (shaderRecord.pipelineLayoutHash != uint32(pipelineLayoutHash))
		return false;
	const uint32 entryPointNameHash = CRC32::Compute(compilationArgs.entryPointName.getData(), compilationArgs.entryPointName.getLength());
	if (shaderRecord.entryPointNameHash != entryPointNameHash)
		return false;
	if (shaderRecord.definesHash != 0)
		return false;
	if (shaderRecord.type != compilationArgs.shaderType)
		return false;
	if (shaderRecord.sourceFileCount == 0)
		return false;

	return true;
}


// BuildDepsTrace::Writer //////////////////////////////////////////////////////////////////////////

void Writer::putRecord(Format::RecordType type,
	const void* bodyData, uintptr bodySize,
	const void* payloadData, uintptr payloadSize,
	bool nullTerminatePayload)
{
	const uintptr bodyOffset = sizeof(Format::RecordHeader);
	const uintptr payloadOffset = bodyOffset + bodySize;
	const uintptr recordSizeUnaligned = payloadOffset + payloadSize + (nullTerminatePayload ? 1 : 0);
	const uint16 recordSize = alignUp<uint16>(XCheckedCastU16(recordSizeUnaligned), Format::RecordAlignment);
	XAssert(recordSize <= Format::MaxRecordSize);

	if (bufferUsedSize + recordSize > BufferSize)
		flush();

	XAssert(bufferUsedSize % Format::RecordAlignment == 0);
	byte* recordData = buffer + bufferUsedSize;
	bufferUsedSize += recordSize;
	memorySet(recordData + recordSize - Format::RecordAlignment, 0, Format::RecordAlignment);

	Format::RecordHeader* recordHeader = (Format::RecordHeader*)recordData;
	recordHeader->size = recordSize;
	recordHeader->hash = 0;
	recordHeader->type = type;
	memoryCopy(recordData + bodyOffset, bodyData, bodySize);
	memoryCopy(recordData + payloadOffset, payloadData, payloadSize);

	const uint16 recordHash = CRC16::Compute(recordData, recordSize);
	recordHeader->hash = recordHash;
}

void Writer::flush()
{
	if (!file.isOpen())
		return;
	if (!bufferUsedSize)
		return;
	XAssert(bufferUsedSize <= BufferSize);
	XAssert(bufferUsedSize % Format::RecordAlignment == 0);

	file.write(buffer, bufferUsedSize);
	bufferUsedSize = 0;

	lastFlushTimestamp = Timer::GetRecord();
}

void Writer::cleanup()
{
	if (file.isOpen())
		file.close();

	if (buffer)
		SystemHeapAllocator::Release(buffer);

	allTrackedSourceFiles.clear();
	sourceFileCache = nullptr;
	buffer = nullptr;
	bufferUsedSize = 0;
	shaderCount = 0;

	lastFlushTimestamp = 0;
}

void Writer::openForWriting(const char* pathCStr, uint32 magic,
	StringViewASCII manifestFilePath, uint64 manifestFileModTime, SourceFileCache& sourceFileCache)
{
	XAssert(!manifestFilePath.isEmpty());

	cleanup();

	file.open(pathCStr, FileAccessMode::Write, FileOpenMode::Override);
	if (!file.isOpen())
		return;

	this->sourceFileCache = &sourceFileCache;
	this->buffer = (byte*)SystemHeapAllocator::Allocate(BufferSize);

	lastFlushTimestamp = Timer::GetRecord();

	// Put file header.
	{
		Format::Header* header = (Format::Header*)buffer;
		header->magic = magic;
		bufferUsedSize = sizeof(Format::Header);
	}

	// Put ManifestFileRecord.
	{
		Format::ManifestFileRecordBody manifestFileRecordBody = {};
		manifestFileRecordBody.modTime0 = uint32(manifestFileModTime);
		manifestFileRecordBody.modTime1 = uint32(manifestFileModTime >> 32);
		manifestFileRecordBody.pathLength = XCheckedCastU16(manifestFilePath.getLength());

		putRecord(Format::RecordType::ManifestFile,
			&manifestFileRecordBody, sizeof(manifestFileRecordBody),
			manifestFilePath.getData(), manifestFilePath.getLength(),
			true);
	}
}

void Writer::addShader(uint64 nameXSH,
	uint64 pipelineLayoutNameXSH, uint64 pipelineLayoutHash,
	const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs,
	const SourceFileHandle* sourceFiles, uint16 sourceFileCount, uint64 compiledBlobModTime)
{
	if (!file.isOpen())
		return;

	XAssert(shaderCount < uint16(-1));
	const uint16 shaderIndex = shaderCount;
	shaderCount++;

	XAssert(sourceFileCount > 0);

	// Collect source file indices while filtering out duplicates.
	InplaceArrayList<uint16, MaxShaderSourceFileCount> shaderSourceFileIndices;
	for (uint16 shaderSourceFileIndex = 0; shaderSourceFileIndex < sourceFileCount; shaderSourceFileIndex++)
	{
		const SourceFileHandle sourceFile = sourceFiles[shaderSourceFileIndex];
		XAssert(sourceFile != SourceFileHandle(0));

		auto it = allTrackedSourceFiles.find(sourceFile);
		if (it != allTrackedSourceFiles.end())
		{
			SourceFileData& sourceFileData = it->second;

			// Check if this source file has already appeared in the list.
			XAssert(sourceFileData.lastDependentShaderIndex <= shaderIndex);
			if (sourceFileData.lastDependentShaderIndex < shaderIndex)
			{
				sourceFileData.lastDependentShaderIndex = shaderIndex;
				if (!shaderSourceFileIndices.isFull())
					shaderSourceFileIndices.pushBack(sourceFileData.index);
			}
		}
		else
		{
			// This is the first time we encounter this source file.
			// Add it to tracked files and put SourceFileRecord.

			XAssert(allTrackedSourceFiles.size() < uint16(-1));
			const uint16 sourceFileIndex = uint16(allTrackedSourceFiles.size());

			allTrackedSourceFiles.insert(
				{ sourceFile, SourceFileData { .index = sourceFileIndex, .lastDependentShaderIndex = shaderIndex } });

			if (!shaderSourceFileIndices.isFull())
				shaderSourceFileIndices.pushBack(sourceFileIndex);

			const StringViewASCII sourceFilePath = sourceFileCache->getFilePath(sourceFile);
			const uint64 sourceFileModTime = sourceFileCache->getFileModTime(sourceFile);

			Format::SourceFileRecordBody sourceFileRecordBody = {};
			sourceFileRecordBody.modTime0 = uint32(sourceFileModTime);
			sourceFileRecordBody.modTime1 = uint32(sourceFileModTime >> 32);
			sourceFileRecordBody.index = sourceFileIndex;
			sourceFileRecordBody.pathLength = XCheckedCastU16(sourceFilePath.getLength());

			putRecord(Format::RecordType::SourceFile,
				&sourceFileRecordBody, sizeof(sourceFileRecordBody),
				sourceFilePath.getData(), sourceFilePath.getLength(),
				true);
		}
	}

	// If shader exceeds source file count limit, no source files should be saved.
	if (shaderSourceFileIndices.isFull())
		shaderSourceFileIndices.clear();

	// Put ShaderRecord.
	{
		Format::ShaderRecordBody shaderRecordBody = {};
		shaderRecordBody.nameXSH0 = uint32(nameXSH);
		shaderRecordBody.nameXSH1 = uint32(nameXSH >> 32);
		shaderRecordBody.pipelineLayoutNameXSH = uint32(pipelineLayoutNameXSH);
		shaderRecordBody.pipelineLayoutHash = uint32(pipelineLayoutHash);
		shaderRecordBody.entryPointNameHash = CRC32::Compute(compilationArgs.entryPointName.getData(), compilationArgs.entryPointName.getLength());
		shaderRecordBody.definesHash = 0;
		shaderRecordBody.compiledBlobFileModTime0 = uint32(compiledBlobModTime);
		shaderRecordBody.compiledBlobFileModTime1 = uint32(compiledBlobModTime >> 32);
		shaderRecordBody.sourceFileCount = shaderSourceFileIndices.getSize();
		shaderRecordBody.type = compilationArgs.shaderType;

		putRecord(Format::RecordType::Shader,
			&shaderRecordBody, sizeof(shaderRecordBody),
			shaderSourceFileIndices.getData(), shaderSourceFileIndices.getByteSize());
	}

	// Flush time threshold is reached.
	if (Timer::GetTimeDelta(lastFlushTimestamp) > MaxFlushIntervalSeconds)
		flush();
}

void Writer::closeAfterSuccessfulBuild(StringViewASCII builtLibraryFilePath, uint64 builtLibraryFileModTime)
{
	XAssert(!builtLibraryFilePath.isEmpty());

	if (!file.isOpen())
		return;

	// Put BuiltLibraryFileRecord.
	{
		Format::BuiltLibraryFileRecordBody builtLibraryFileRecordBody = {};
		builtLibraryFileRecordBody.modTime0 = uint32(builtLibraryFileModTime);
		builtLibraryFileRecordBody.modTime1 = uint32(builtLibraryFileModTime >> 32);
		builtLibraryFileRecordBody.pathLength = XCheckedCastU16(builtLibraryFilePath.getLength());

		putRecord(Format::RecordType::BuiltLibraryFile,
			&builtLibraryFileRecordBody, sizeof(builtLibraryFileRecordBody),
			builtLibraryFilePath.getData(), builtLibraryFilePath.getLength(),
			true);
	}

	flush();
	cleanup();
}

void Writer::closeAfterFailedBuild()
{
	flush();
	cleanup();
}
