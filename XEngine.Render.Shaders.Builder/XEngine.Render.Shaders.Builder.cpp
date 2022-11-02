#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.CRC.h>	// TODO: Remove
#include <XLib.String.h>
#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>
#include <XLib.Text.h>

#include <XEngine.Render.HAL.BlobFormat.h> // TODO: Remove
#include <XEngine.Render.HAL.ShaderCompiler.h>
#include <XEngine.Render.Shaders.PackFormat.h>

#include "XEngine.Render.Shaders.Builder.h"

#include "XEngine.Render.Shaders.Builder.BuildDescriptionLoader.h"

using namespace XLib;
using namespace XEngine::Render::HAL::ShaderCompiler;
using namespace XEngine::Render::Shaders;
using namespace XEngine::Render::Shaders::Builder_;

// TODO: Replace with `HashMap` when ready
template <typename Key, typename Value>
class Map
{
private:
	struct Pair
	{
		Key key;
		Value value;
	};

private:
	ArrayList<Pair> pairs;

public:
	inline bool insert(const Key& key, const Value& value)
	{
		for (const Pair& pair : pairs)
		{
			if (pair.key == key)
				return false;
		}
		pairs.pushBack(Pair { key, value });
		return true;
	}

	inline Value* find(const Key& key)
	{
		for (Pair& pair : pairs)
		{
			if (pair.key == key)
				return &pair.value;
		}
		return nullptr;
	}
};

uint64 GetBlobLongHash(const Blob& blob) // TODO: Remove this hack.
{
	XAssert(blob.isInitialized());
	return CRC64::Compute(blob.getData(), blob.getSize());
}

uint32 GetBlobChecksum(const Blob& blob) // TODO: Remove this hack.
{
	using namespace XEngine::Render::HAL::BlobFormat::Data;

	XAssert(blob.isInitialized());
	const GenericBlobHeader* header = (const GenericBlobHeader*)blob.getData();
	return header->checksum;
}

void Builder::run(const char* packPath)
{
	sourceCache.initialize("../XEngine.Render.Shaders/");

	BuildDescriptionLoader descriptionLoader(descriptorSetLayoutList, pipelineLayoutList, pipelineList, shaderList, sourceCache);
	if (!descriptionLoader.load("../XEngine.Render.Shaders/.LibraryIndex.txt"))
		return;

	// Compile.

	for (DescriptorSetLayout& descriptorSetLayout : descriptorSetLayoutList)
	{
		TextWriteFmtStdOut("Compiling descriptor set layout \"", descriptorSetLayout.getName(), "\"\n");
	
		if (!descriptorSetLayout.compile())
		{
			TextWriteFmtStdOut("Failed to compile descriptor set layout \"", descriptorSetLayout.getName(), "\"\n");
			return;
		}
	}

	for (PipelineLayout& pipelineLayout : pipelineLayoutList)
	{
		TextWriteFmtStdOut("Compiling pipeline layout \"", pipelineLayout.getName(), "\"\n");

		if (!pipelineLayout.compile())
		{
			TextWriteFmtStdOut("Failed to compile pipeline layout \"", pipelineLayout.getName(), "\"\n");
			return;
		}
	}

	for (Shader& shader : shaderList)
	{
		InplaceStringASCIIx1024 shaderName;
		StringViewASCII shaderTypeString = "XS"; // TODO: ...
		TextWriteFmt(shaderName,
			'[', shaderTypeString, ']',
			shader.getSource().getLocalPath(), ':', shader.getEntryPointName(),
			"(layout=", shader.getPipelineLayout().getName(), ')');

		TextWriteFmtStdOut("Compiling shader \"", StringViewASCII(shaderName), "\"\n"); // TODO: Remove cast to string view.

		if (!shader.compile())
		{
			TextWriteFmtStdOut("Failed to compile shader \"", StringViewASCII(shaderName), "\"\n");
			return;
		}
	}

	for (Pipeline& pipeline : pipelineList)
	{
		TextWriteFmtStdOut("Compiling pipeline \"", pipeline.getName(), "\"\n");

		if (!pipeline.compile())
		{
			TextWriteFmtStdOut("Failed to compile pipeline \"", pipeline.getName(), "\"\n");
			return;
		}
	}

	// Compose pack.

	TextWriteFmtStdOut("Composing shaderpack \"", packPath, "\"\n");

	using namespace PackFormat;

	ArrayList<PipelineLayoutRecord> pipelineLayoutRecords;
	ArrayList<PipelineRecord> pipelineRecords;
	ArrayList<BlobRecord> bytecodeBlobRecords;

	ArrayList<Blob, uint16> genericBlobs;
	ArrayList<Blob, uint16> bytecodeBlobs;

	Map<uint64, uint16> pipelineLayoutNameXSHToGenericBlobIdxMap;
	Map<uint64, uint16> bytecodeBlobLongHashToIdxMap; // Used for blobs deduplication.

	uint32 genericBlobsSizeAccum = 0;

	pipelineLayoutRecords.reserve(pipelineLayoutList.getSize());
	for (PipelineLayout& pipelineLayout : pipelineLayoutList) // Iterating in order of name hash increase
	{
		const Blob& blob = pipelineLayout.getCompiledBlob();

		PipelineLayoutRecord& pipelineLayoutRecord = pipelineLayoutRecords.emplaceBack();
		pipelineLayoutRecord = {};
		pipelineLayoutRecord.nameXSH = pipelineLayout.getNameXSH();

		pipelineLayoutRecord.blob.offset = genericBlobsSizeAccum;
		pipelineLayoutRecord.blob.size = blob.getSize();
		pipelineLayoutRecord.blob.checksum = GetBlobChecksum(blob);
		genericBlobsSizeAccum += blob.getSize();

		const uint16 blobIndex = genericBlobs.getSize();
		genericBlobs.emplaceBack(blob.addReference());

		pipelineLayoutNameXSHToGenericBlobIdxMap.insert(pipelineLayout.getNameXSH(), blobIndex);
	}

	pipelineRecords.reserve(pipelineList.getSize());
	for (Pipeline& pipeline : pipelineList) // Iterating in order of name hash increase
	{
		// TODO: Check if element does not exist
		const uint16 pipelineLayoutIndex =
			*pipelineLayoutNameXSHToGenericBlobIdxMap.find(pipeline.getPipelineLayout().getNameXSH());

		const bool isGraphics = pipeline.isGraphicsPipeline();

		PipelineRecord& pipelineRecord = pipelineRecords.emplaceBack();
		pipelineRecord = {};
		pipelineRecord.nameXSH = pipeline.getNameXSH();
		pipelineRecord.pipelineLayoutIndex = pipelineLayoutIndex;
		pipelineRecord.isGraphics = isGraphics;

		auto deduplicateBytecodeBlob = [&bytecodeBlobLongHashToIdxMap, &bytecodeBlobs](const Blob& blob) -> uint16
		{
			const uint64 blobLongHash = GetBlobLongHash(blob);

			const uint16* existingBlobIndexIt = bytecodeBlobLongHashToIdxMap.find(blobLongHash);
			if (existingBlobIndexIt)
				return *existingBlobIndexIt;
			
			const uint16 blobIndex = bytecodeBlobs.getSize();
			bytecodeBlobs.emplaceBack(blob.addReference());
			bytecodeBlobLongHashToIdxMap.insert(blobLongHash, blobIndex);
			return blobIndex;
		};

		uint8 bytecodeBlobCount = 0;

		if (isGraphics)
		{
			const HAL::ShaderCompiler::GraphicsPipelineCompilationResult& compiled = pipeline.getCompiledGraphics();

			// Base blob.
			{
				pipelineRecord.graphicsBaseBlob.offset = genericBlobsSizeAccum;
				pipelineRecord.graphicsBaseBlob.size = compiled.baseBlob.getSize();
				pipelineRecord.graphicsBaseBlob.checksum = GetBlobChecksum(compiled.baseBlob);
				genericBlobsSizeAccum += compiled.baseBlob.getSize();

				genericBlobs.emplaceBack(compiled.baseBlob.addReference());
			}

			// Bytecode blobs.
			XAssert(countof(compiled.bytecodeBlobs) <= countof(pipelineRecord.bytecodeBlobIndices));
			for (uint8 i = 0; i < countof(compiled.bytecodeBlobs); i++)
			{
				const Blob& bytecodeBlob = compiled.bytecodeBlobs[i];
				if (!bytecodeBlob.isInitialized())
					break;
				pipelineRecord.bytecodeBlobIndices[i] = deduplicateBytecodeBlob(bytecodeBlob);
				bytecodeBlobCount++;
			}
		}
		else
		{
			pipelineRecord.bytecodeBlobIndices[0] = deduplicateBytecodeBlob(pipeline.getCompiledComputeShaderBlob());
			bytecodeBlobCount = 1;
		}

		for (uint8 i = bytecodeBlobCount; i < countof(pipelineRecord.bytecodeBlobIndices); i++)
			pipelineRecord.bytecodeBlobIndices[i] = uint16(-1);
	}

	const uint32 genericBlobsTotalSize = genericBlobsSizeAccum;

	const uint32 bytecodeBlobsBaseOffset = genericBlobsTotalSize;
	uint32 bytecodeBlobsOffsetAccum = 0;

	bytecodeBlobRecords.reserve(bytecodeBlobs.getSize());
	for (const Blob& blob : bytecodeBlobs)
	{
		BlobRecord& record = bytecodeBlobRecords.emplaceBack();
		record = {};
		record.offset = bytecodeBlobsOffsetAccum + bytecodeBlobsBaseOffset;
		record.size = blob.getSize();
		record.checksum = GetBlobChecksum(blob);
		bytecodeBlobsOffsetAccum += blob.getSize();
	}

	const uint32 bytecodeBlobsTotalSize = bytecodeBlobsOffsetAccum;

	const uint32 pipelineLayoutRecordsSizeBytes = pipelineLayoutRecords.getSize() * sizeof(PipelineLayoutRecord);
	const uint32 pipelineRecordsSizeBytes = pipelineRecords.getSize() * sizeof(PipelineRecord);
	const uint32 bytecodeBlobRecordsSizeBytes = bytecodeBlobRecords.getSize() * sizeof(BlobRecord);
	const uint32 blobsDataOffset =
		sizeof(PackHeader) +
		pipelineLayoutRecordsSizeBytes +
		pipelineRecordsSizeBytes +
		bytecodeBlobRecordsSizeBytes;

	File file;
	file.open(packPath, FileAccessMode::Write, FileOpenMode::Override);

	PackHeader header = {};
	header.signature = PackSignature;
	header.version = PackCurrentVersion;
	header.pipelineLayoutCount = uint16(pipelineLayoutRecords.getSize());
	header.pipelineCount = uint16(pipelineRecords.getSize());
	header.bytecodeBlobCount = uint16(bytecodeBlobRecords.getSize());
	header.blobsDataOffset = blobsDataOffset;
	file.write(header);

	file.write(pipelineLayoutRecords.getData(), pipelineLayoutRecordsSizeBytes);
	file.write(pipelineRecords.getData(), pipelineRecordsSizeBytes);
	file.write(bytecodeBlobRecords.getData(), bytecodeBlobRecordsSizeBytes);

	uint32 genericBlobsTotalSizeCheck = 0;
	for (const Blob& blob : genericBlobs)
	{
		file.write(blob.getData(), blob.getSize());
		genericBlobsTotalSizeCheck += blob.getSize();
	}

	uint32 bytecodeBlobsTotalSizeCheck = 0;
	for (const Blob& blob : bytecodeBlobs)
	{
		file.write(blob.getData(), blob.getSize());
		bytecodeBlobsTotalSizeCheck += blob.getSize();
	}

	XAssert(genericBlobsTotalSizeCheck == genericBlobsTotalSize);
	XAssert(bytecodeBlobsTotalSizeCheck == bytecodeBlobsTotalSize);

	file.close();

	TextWriteFmtStdOut("Build successful\n");
}

#if 0

static inline ShaderType ShaderTypeFromString(const StringView& str)
{
	if (str.getLength() != 2 || str[1] != 'S')
		return ShaderType::Undefined;

	switch (str[0])
	{
		case 'C':	return ShaderType::Compute;
		case 'V':	return ShaderType::Vertex;
		case 'A':	return ShaderType::Amplification;
		case 'M':	return ShaderType::Mesh;
		case 'P':	return ShaderType::Pixel;
	}
	return ShaderType::Undefined;
}

static void CheckShaderDependenciesTimestampsAndUpdateCompilationFlag(Shader& shader,
	SourcesCache& sourcesCache, const char* rootPath, TimePoint& upToDateObjectWriteTime)
{
	upToDateObjectWriteTime = InvalidTimePoint;

	// Already marked for compilation
	if (shader.isCompilationRequired())
		return;

	const TimePoint objectWriteTime = ...;
	if (objectWriteTime == InvalidTimePoint)
	{
		shader.setCompilationRequired();
		return;
	}

	const SourcesCacheEntryId sourceMainId = shader.getSourceMain();
	const TimePoint sourceMainWriteTime = sourcesCache.getEntry(sourceMainId).checkWriteTime(rootPath);
	if (sourceMainWriteTime == InvalidTimePoint)
	{
		shader.setCompilationRequired();
		return;
	}

	// Go through all dependencies checking timestamps
	TimePoint latestSourceDependencyWriteTime = 0;
	for (uint16 i = 0; i < shader.getSourceDependencyCount(); i++)
	{
		const SourcesCacheEntryId id = shader.getSourceDependency(i);
		const TimePoint writeTime = sourcesCache.getEntry(id).checkWriteTime(rootPath);

		// InvalidTimePoint will propagate here
		latestSourceDependencyWriteTime = max<TimePoint>(latestSourceDependencyWriteTime, writeTime);
	}

	const TimePoint latestSourceWriteTime = max(sourceMainWriteTime, latestSourceDependencyWriteTime);

	if (latestSourceWriteTime > objectWriteTime)
	{
		shader.setCompilationRequired();
		return;
	}

	upToDateObjectWriteTime = objectWriteTime;
}

void Builder::build()
{
	const bool rebuildAll = !metadataLoadResult;
	if (rebuildAll)
	{
		for (ShadersListEntry& shader : shadersList)
			shadersToCompile.pushBack(&shader);
	}
	else
	{
		const TimePoint packWriteTime = FileSystem::GetFileLastWriteTime(outputFilePath.cstr());

		// Relink pack if it does not exist
		relinkPack |= packWriteTime == InvalidTimePoint;

		TimePoint latestObjectWriteTime = 0;

		for (ShadersListEntry& shader : shadersList)
		{
			TimePoint upToDateObjectWriteTime = 0;
			CheckIfShaderRequiresCompilation(shader, sourcesCache, rootPath.cstr(), upToDateObjectWriteTime);

			if (shader.isCompilationRequired())
				shadersToCompile.pushBack(&shader);
			else
				latestObjectWriteTime = max(latestObjectWriteTime, upToDateObjectWriteTime);
		}

		// Relink pack if all shaders are up to date but pack is out of date
		if (shadersToCompile.isEmpty())
			relinkPack |= latestObjectWriteTime > packWriteTime;
	}

	relinkPack |= !shadersToCompile.isEmpty();

	for (Shader& shader : shadersList)
	{
		const PipelineLayout& pipelineLayout = pipelineLayoutsList.getEntry(shader.getPipelineLayout());
		Host::CompileShader(Platform::D3D12, pipelineLayout.getCompiled(), shader.getType(), ...);
	}
}

#endif
