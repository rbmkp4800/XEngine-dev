#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.String.h>
#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>
#include <XLib.Text.h>

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

#if 0

bool Builder::loadTargetDescription(const char* targetDescriptionPath)
{
	File targetDescriptionFile;
	targetDescriptionFile.open(targetDescriptionPath, FileAccessMode::Read, FileOpenMode::OpenExisting);
	if (!targetDescriptionFile.isInitialized())
		return false;

	// TODO: Check for U32 overflow.
	const uint32 targetDescriptionFileSize = uint32(targetDescriptionFile.getSize());

	String targetDescriptionText;
	targetDescriptionText.resizeUnsafe(targetDescriptionFileSize);

	targetDescriptionFile.read(targetDescriptionText.getMutableData(), targetDescriptionFileSize);
	targetDescriptionFile.close();

	TargetDescriptionLoader loader;
}

#endif

void Builder::run(const char* packPath)
{
	sourceCache.initialize("..\\XEngine.Render\\Shaders\\");

	BuildDescriptionLoader descriptionLoader(pipelineLayoutList, pipelineList, shaderList, sourceCache);
	descriptionLoader.load("..\\XEngine.Render.Shaders\\_BuildDescription.txt");

#if 0

	BindPointDesc bp0 = {};
	bp0.name = "name0";
	bp0.type = HAL::PipelineBindPointType::ConstantBuffer;
	bp0.shaderVisibility = HAL::ShaderCompiler::PipelineBindPointShaderVisibility::All;

	PipelineLayout& testLayout = *pipelineLayoutsList.createEntry("TestPipelineLayout", &bp0, 1);

	pipelinesList.createGraphicsPipeline(
		"TestGfxPipeline",
		testLayout,
		Builder_::GraphicsPipelineDesc {
			.vertexShader = shadersList.findOrCreateEntry(ShaderType::Vertex, sourcesCache.findOrCreateEntry("UIColorVS.hlsl"), testLayout),
			.renderTargetsFormats = { HAL::TexelViewFormat::R8G8B8A8_UNORM },
		});
#endif

	// Compile.

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
		TextWriteFmtStdOut("Compiling shader \"", "shader.getName()", "\"\n");

		if (!shader.compile())
		{
			TextWriteFmtStdOut("Failed to compile shader \"", "shader.getName()", "\"\n");
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
	ArrayList<ObjectRecord> bytecodeObjectRecords;

	ArrayList<Object, uint16> genericObjects;
	ArrayList<Object, uint16> bytecodeObjects;

	Map<uint64, uint16> pipelineLayoutNameCRCToGenericObjectIdxMap;
	Map<ObjectHash, uint16> bytecodeObjectHashToIdxMap;

	uint32 genericObjectsOffsetAccum = 0;

	pipelineLayoutRecords.reserve(pipelineLayoutList.getSize());
	for (PipelineLayout& pipelineLayout : pipelineLayoutList) // Iterating in order of name CRC increase
	{
		const Object& object = pipelineLayout.getCompiled().getObject();

		PipelineLayoutRecord& record = pipelineLayoutRecords.pushBack(PipelineLayoutRecord{});
		record.nameCRC = pipelineLayout.getNameCRC();

		record.object.offset = genericObjectsOffsetAccum;
		record.object.size = object.getSize();
		record.object.crc = object.getCRC();
		genericObjectsOffsetAccum += object.getSize();

		const uint16 objectIndex = genericObjects.getSize();
		genericObjects.emplaceBack(object.clone());

		pipelineLayoutNameCRCToGenericObjectIdxMap.insert(pipelineLayout.getNameCRC(), objectIndex);
	}

	pipelineRecords.reserve(pipelineList.getSize());
	for (Pipeline& pipeline : pipelineList) // Iterating in order of name CRC increase
	{
		// TODO: Check if element does not exist
		const uint16 pipelineLayoutIndex =
			*pipelineLayoutNameCRCToGenericObjectIdxMap.find(pipeline.getPipelineLayout().getNameCRC());

		const bool isGraphics = pipeline.isGraphicsPipeline();

		PipelineRecord& pipelineRecord = pipelineRecords.pushBack(PipelineRecord{});
		pipelineRecord.nameCRC = pipeline.getNameCRC();
		pipelineRecord.pipelineLayoutIndex = pipelineLayoutIndex;
		pipelineRecord.isGraphics = isGraphics;

		auto deduplicateBytecodeObject = [&bytecodeObjectHashToIdxMap, &bytecodeObjects](const Object& object) -> uint16
		{
			const uint16* existingObjectIndexIt = bytecodeObjectHashToIdxMap.find(object.getHash());
			if (existingObjectIndexIt)
				return *existingObjectIndexIt;
			
			const uint16 objectIndex = bytecodeObjects.getSize();
			bytecodeObjects.emplaceBack(object.clone());
			bytecodeObjectHashToIdxMap.insert(object.getHash(), objectIndex);
			return objectIndex;
		};

		uint8 bytecodeObjectCount = 0;

		if (isGraphics)
		{
			const CompiledGraphicsPipeline& compiled = pipeline.getCompiledGraphics();

			// Base object
			{
				const Object& baseObject = compiled.getBaseObject();

				pipelineRecord.graphicsBaseObject.offset = genericObjectsOffsetAccum;
				pipelineRecord.graphicsBaseObject.size = baseObject.getSize();
				pipelineRecord.graphicsBaseObject.crc = baseObject.getCRC();
				genericObjectsOffsetAccum += baseObject.getSize();

				genericObjects.emplaceBack(baseObject.clone());
			}

			// Bytecode objects
			XAssert(compiled.getBytecodeObjectCount() < countof(pipelineRecord.bytecodeObjectsIndices));
			for (uint8 i = 0; i < compiled.getBytecodeObjectCount(); i++)
				pipelineRecord.bytecodeObjectsIndices[i] = deduplicateBytecodeObject(compiled.getBytecodeObject(i));
			bytecodeObjectCount = compiled.getBytecodeObjectCount();
		}
		else
		{
			pipelineRecord.bytecodeObjectsIndices[0] = deduplicateBytecodeObject(pipeline.getCompiledCompute().getObject());
			bytecodeObjectCount = 1;
		}

		for (uint8 i = bytecodeObjectCount; i < countof(pipelineRecord.bytecodeObjectsIndices); i++)
			pipelineRecord.bytecodeObjectsIndices[i] = uint16(-1);
	}

	const uint32 genericObjectsTotalSize = genericObjectsOffsetAccum;

	const uint32 bytecodeObjectsBaseOffset = genericObjectsTotalSize;
	uint32 bytecodeObjectOffsetAccum = 0;

	bytecodeObjectRecords.reserve(bytecodeObjects.getSize());
	for (const Object& object : bytecodeObjects)
	{
		ObjectRecord& record = bytecodeObjectRecords.pushBack(ObjectRecord{});
		record.offset = bytecodeObjectOffsetAccum + bytecodeObjectsBaseOffset;
		record.size = object.getSize();
		record.crc = object.getCRC();
		bytecodeObjectOffsetAccum += object.getSize();
	}

	const uint32 bytecodeObjectsTotalSize = bytecodeObjectOffsetAccum;

	const uint32 pipelineLayoutRecordsSizeBytes = pipelineLayoutRecords.getSize() * sizeof(PipelineLayoutRecord);
	const uint32 pipelineRecordsSizeBytes = pipelineRecords.getSize() * sizeof(PipelineRecord);
	const uint32 bytecodeObjectRecordsSizeBytes = bytecodeObjectRecords.getSize() * sizeof(ObjectRecord);
	const uint32 objectsBaseOffset = sizeof(PackHeader) +
		pipelineLayoutRecordsSizeBytes + pipelineRecordsSizeBytes + bytecodeObjectRecordsSizeBytes;

	File file;
	file.open(packPath, FileAccessMode::Write, FileOpenMode::Override);

	PackHeader header = {};
	header.signature = PackSignature;
	header.version = PackCurrentVersion;
	header.pipelineLayoutCount = uint16(pipelineLayoutRecords.getSize()); // TODO: Check overflows
	header.pipelineCount = uint16(pipelineRecords.getSize());
	header.bytecodeObjectCount = uint16(bytecodeObjectRecords.getSize());
	header.objectsBaseOffset = objectsBaseOffset;
	file.write(header);

	file.write(pipelineLayoutRecords.getData(), pipelineLayoutRecordsSizeBytes);
	file.write(pipelineRecords.getData(), pipelineRecordsSizeBytes);
	file.write(bytecodeObjectRecords.getData(), bytecodeObjectRecordsSizeBytes);

	uint32 genericObjectsTotalSizeCheck = 0;
	for (const Object& object : genericObjects)
	{
		file.write(object.getData(), object.getSize());
		genericObjectsTotalSizeCheck += object.getSize();
	}

	uint32 bytecodeObjectsTotalSizeCheck = 0;
	for (const Object& object : bytecodeObjects)
	{
		file.write(object.getData(), object.getSize());
		bytecodeObjectsTotalSizeCheck += object.getSize();
	}

	XAssert(genericObjectsTotalSizeCheck == genericObjectsTotalSize);
	XAssert(bytecodeObjectsTotalSizeCheck == bytecodeObjectsTotalSize);

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
