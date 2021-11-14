#include <XLib.h>
#include <XLib.JSON.h>
#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>
#include <XEngine.Render.Shaders.PackFormat.h>

#include "XEngine.Render.Shaders.Builder.h"

using namespace XLib;
using namespace XEngine::Render::HAL::ShaderCompiler;
using namespace XEngine::Render::Shaders;
using namespace XEngine::Render::Shaders::Builder_;

bool Builder::loadIndex(const char* indexPath)
{
	pipelineLayoutsList.createEntry("TestPipelineLayout", ...);
	pipelinesList.createGraphicsPipeline(
		"TestGfxPipeline",
		pipelineLayoutsList.findEntry("TestPipelineLayout"),
		Builder_::GraphicsPipelineDesc {
			.vertexShader = shadersList.findOrCreateEntry(
				ShaderType::Vertex,
				sourcesCache.findOrCreateEntry("test.hlsl")),
			.renderTargetsFormats = { HAL::TexelFormat::R8G8B8A8_UNORM },
		});
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

void Builder::storePack(const char* packagePath)
{
	using namespace PackFormat;

	struct PipelineBytecodeObjectsDeduplicationListItem
	{
		BinaryBlob* blob;
		uint32 blobsMapEntryIndex;
	};

	ArrayList<PipelineDesc, uint32, false> serializedPipelines;
	serializedPipelines.reserve(pipelinesList.getSize());

	ArrayList<PipelineBlobsDeduplicationListItem, uint32, false> pipelineBlobsDeduplicationList;

	uint32 pipelineBlobsMapEntryIndexAccum = 0;
	for (const Pipeline& pipeline : pipelinesList)
	{
		const CompiledPipeline& compiledPipeline = pipeline.getCompiled();

		PipelineDesc& serializedPipelineDesc = serializedPipelines.emplaceBack();
		serializedPipelineDesc.nameCRC = pipeline.getNameCRC();
		serializedPipelineDesc.pipelineLayoutIndex = ...;
		serializedPipelineDesc.type = pipeline.getType();
		serializedPipelineDesc.binaryBlobCount = pipelineBlobs.getSize();
		serializedPipelineDesc.binaryBlobsMapOffset = pipelineBlobsMapEntryIndexAccum;

		for (BinaryBlob* blob : pipelineBlobs)
		{
			PipelineBlobsDeduplicationListItem& item = pipelineBlobsDeduplicationList.emplaceBack();
			item.blob = blob;
			item.blobsMapEntryIndex = pipelineBlobsMapEntryIndexAccum;
			pipelineBlobsMapEntryIndexAccum++;
		}
	}
	pipelineBlobsDeduplicationList.compact();

	const uint32 pipelineBlobsMapSize = pipelineBlobsMapEntryIndexAccum;

	Sort(pipelineBlobsDeduplicationList, ...);

	ArrayList<uint32, uint32, false> pipelineBlobsMap;
	pipelineBlobsMap.resize(pipelineBlobsMapSize);

	{
		BinaryBlob* prevDeduplicatedBlob = nullptr;
		uint32 prevDeduplicatedBlobsMapEntryIndex = 0;
		for (PipelineBlobsDeduplicationListItem& item : pipelineBlobsDeduplicationList)
		{
			if (prevDeduplicatedBlob != item.blob)
			{
				prevDeduplicatedBlob = item.blob;
				prevDeduplicatedBlobsMapEntryIndex = item.blobsMapEntryIndex;
			}

			ASSERT(item.blobsMapEntryIndex < pipelineBlobsMapSize);
			pipelineBlobsMap[item.blobsMapEntryIndex] = prevDeduplicatedBlobsMapEntryIndex;
		}
	}

	ArrayList<BinaryBlobDesc> serializedBinaryBlobs;

	uint32 blobsTotalSizeAccumulator = 0;

	for (const PipelineLayout& PipelineLayout : pipelineLayoutsList)
	{
		const CompiledPipelineLayout& compiledLayout = PipelineLayout.getCompiled();

		BinaryBlobDesc desc = {};
		desc.offset = blobsTotalSizeAccumulator;
		desc.size = compiledLayout.getBinaryBlobSize();
		serializedBinaryBlobs.pushBack(desc);

		blobsTotalSizeAccumulator += compiledLayout.getBinaryBlobSize();
	}

	File file;
	file.open(packagePath, FileAccessMode::Write, FileOpenMode::Override);

	Header header = {};
	header.signature = Signature;
	header.version = CurrentVersion;
	header.platformFlags = 0;
	header.pipelineLayoutCount = 0;
	header.pipelineCount = 0;
	header.binaryBlobCount = 0;
	file.write(header);

	file.close();
}

#endif
