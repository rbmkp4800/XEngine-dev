#include <XLib.h>
#include <XLib.Containers.FlatHashMap.h>
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
			.vertexShader = shadersList.findOrCreateEntry(ShaderType::Vertex, sourcesCache.findOrCreateEntry("test.hlsl")),
			.renderTargetsFormats = { HAL::TexelFormat::R8G8B8A8_UNORM },
		});
}

void Builder::build()
{
	for (PipelineLayout& pipelineLayout : pipelineLayoutsList)
		pipelineLayout.compile();

	for (Shader& shader : shadersList)
		shader.compile();
}

void Builder::composePack(const char* packPath)
{
	using namespace PackFormat;

	ArrayList<Object> genericObjects;
	uint32 genericObjectsOffsetAccum = 0;

	ArrayList<PipelineLayoutRecord> pipelineLayoutRecords;
	FlatHashMap<uint64, uint16> pipelineLayoutNameCRCToGenericObjectIdxMap;

	pipelineLayoutRecords.reserve(pipelineLayoutsList.getSize());
	for (PipelineLayout& pipelineLayout : pipelineLayoutsList)
	{
		const Object& object = pipelineLayout.getCompiled().getObject();

		PipelineLayoutRecord& record = pipelineLayoutRecords.pushBack(PipelineLayoutRecord{});
		record.nameCRC = pipelineLayout.getNameCRC();

		record.object.offset = genericObjectsOffsetAccum;
		record.object.size = object.getSize();
		record.object.crc = object.getCRC();
		genericObjectsOffsetAccum += object.getSize();

		const uint32 objectIndexU32 = genericObjects.getSize();
		genericObjects.emplaceBack(object.clone());

		XEAssert(objectIndexU32 < uint16(-1));
		const uint16 objectIndex = uint16(objectIndexU32);

		pipelineLayoutNameCRCToGenericObjectIdxMap.insert(pipelineLayout.getNameCRC(), uint16(objectIndex));
	}

	ArrayList<Object> bytecodeObjects;
	FlatHashMap<ObjectHash, uint16> bytecodeObjectHashToIdxMap;

	ArrayList<PipelineRecord> pipelineRecords;
	pipelineRecords.reserve(pipelinesList.getSize());
	for (Pipeline& pipeline : pipelinesList)
	{
		const PipelineLayout& pipelineLayout = pipelineLayoutsList.getEntry(pipeline.getPipelineLayout());
		// TODO: Check if element does not exist
		const uint16 pipelineLayoutIndex = *pipelineLayoutNameCRCToGenericObjectIdxMap.find(pipelineLayout.getNameCRC());

		const bool isGraphics = pipeline. ...;

		PipelineRecord& record = pipelineRecords.pushBack(PipelineRecord{});
		record.nameCRC = pipeline.getNameCRC();
		record.pipelineLayoutIndex = pipelineLayoutIndex;
		record.isGraphics = isGraphics;

		if (isGraphics)
		{
			const CompiledGraphicsPipeline& compiled = pipeline.getCompiledGraphics();
			const Object& baseObject = compiled.getBaseObject();

			record.graphicsBaseObject.offset = genericObjectsOffsetAccum;
			record.graphicsBaseObject.size = baseObject.getSize();
			record.graphicsBaseObject.crc = baseObject.getCRC();
			genericObjectsOffsetAccum += baseObject.getSize();

			genericObjects.emplaceBack(baseObject.clone());
		}
		else
		{
			const CompiledShader& compiled =  pipeline.getCompiledCompute();
			record.bytecodeObjectsIndices[0] = ...;
		}
	}

	uint32 bytecodeObjectOffsetAccum = 0;
	bytecodeObjectOffsetAccum += genericObjectsOffsetAccum; // Bytecode objects go after generic objects

	ArrayList<ObjectRecord> bytecodeObjectRecords;
	bytecodeObjectRecords.reserve(bytecodeObjects.getSize());
	for (const Object& object : bytecodeObjects)
	{
		ObjectRecord& record = bytecodeObjectRecords.pushBack(ObjectRecord{});
		record.offset = bytecodeObjectOffsetAccum;
		record.size = object.getSize();
		record.crc = object.getCRC();
		bytecodeObjectOffsetAccum += object.getSize();
	}

	File file;
	file.open(packPath, FileAccessMode::Write, FileOpenMode::Override);

	PackHeader header = {};
	header.signature = PackSignature;
	header.version = PackCurrentVersion;
	header.pipelineLayoutCount = pipelineLayoutRecords.getSize();
	header.pipelineCount = pipelineRecords.getSize();
	header.bytecodeObjectCount = bytecodeObjectRecords.getSize();
	file.write(header);

	file.write(pipelineLayoutRecords.getData(), pipelineLayoutRecords.getSize() * sizeof(PipelineLayoutRecord));
	file.write(pipelineRecords.getData(), pipelineRecords.getSize() * sizeof(PipelineRecord));
	file.write(bytecodeObjectRecords.getData(), bytecodeObjectRecords.getSize() * sizeof(ObjectRecord));

	file.close();
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
