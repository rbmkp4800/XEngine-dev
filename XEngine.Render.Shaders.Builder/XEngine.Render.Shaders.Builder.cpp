#include <XLib.h>
#include <XLib.JSON.h>
#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>
#include <XEngine.Render.Shaders.PackFile.h>

#include "XEngine.Render.Shaders.Builder.h"

using namespace XLib;
using namespace XEngine::Render::HAL::ShaderCompiler;
using namespace XEngine::Render::Shaders;
using namespace XEngine::Render::Shaders::Builder_;

static inline ShaderType ShaderTypeFromString(const StringView& str)
{
	if (str.getLength() != 2 || str[1] != 'S')
		return ShaderType::None;

	switch (str[0])
	{
		case 'C':	return ShaderType::CS;
		case 'V':	return ShaderType::VS;
		case 'M':	return ShaderType::MS;
		case 'A':	return ShaderType::AS;
		case 'P':	return ShaderType::PS;
	}
	return ShaderType::None;
}

#if 0

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

#endif

bool Builder::loadIndex(const char* shadersListFilePath)
{
	File file;
	file.open(shadersListFilePath, FileAccessMode::Read);

	const uint64 fileSize = file.getSize();

	// TODO: This should be some kind of safe ptr, so we can release on return
	char* fileContent = (char*)SystemHeapAllocator::Allocate(fileSize);

	uint32 readSize = 0;
	const bool readResult = file.read(fileContent, fileSize, readSize);

	file.close();

	if (!readResult || readSize != fileSize)
		return false;

	using JSONNodeId = JSONDocumentTree::NodeId;
	static constexpr JSONNodeId JSONRootNodeId = JSONDocumentTree::RootNodeId;
	static constexpr JSONNodeId JSONInvalidNodeId = JSONDocumentTree::InvalidNodeId;

	JSONDocumentTree jsonTree;
	if (!jsonTree.parse(fileContent))
		return false;

	if (jsonTree.isEmpty() || !jsonTree.isArray(JSONRootNodeId))
		return false;

	for (JSONNodeId jsonRootArrayIt = jsonTree.getArrayBegin(JSONRootNodeId);
		jsonRootArrayIt != JSONInvalidNodeId;
		jsonRootArrayIt = jsonTree.getArrayNext(jsonRootArrayIt))
	{
		if (!jsonTree.isObject(jsonRootArrayIt))
			return false;

		StringView shaderName = {};
		StringView shaderTypeStr = {};
		StringView shaderSourcePath = {};
		StringView shaderEntryPoint = {};

		if (!jsonTree.getStringProperty(jsonRootArrayIt, "name", shaderName))
			return false;
		if (!jsonTree.getStringProperty(jsonRootArrayIt, "type", shaderTypeStr))
			return false;
		if (!jsonTree.getStringProperty(jsonRootArrayIt, "src", shaderSourcePath))
			return false;

		if (shaderName.getLength() > Shader::NameLengthLimit)
			return false;
		if (shaderEntryPoint.getLength() > Shader::EntryPointNameLengthLimit)
			return false;
		if (shaderSourcePath.getLength() > SourcesCacheEntry::LocalPathLengthLimit)
			return false;

		const ShaderType shaderType = ShaderTypeFromString(shaderTypeStr);
		if (shaderType == ShaderType::None)
			return false;

		const JSONNodeId entryPointNameNodeId = jsonTree.getProperty(jsonRootArrayIt, "entryPoint");
		if (entryPointNameNodeId != JSONInvalidNodeId)
		{
			if (!jsonTree.isString(entryPointNameNodeId))
				return false;
			shaderEntryPoint = jsonTree.asString(entryPointNameNodeId);
		}

		const SourcesCacheEntryId mainSourceId = sourcesCache.findOrCreateEntry(shaderSourcePath);

		Shader* shader = shadersList.createEntry(shaderName, shaderType, mainSourceId);
		if (!shader)
		{
			// Duplicate shader
			return false;
		}
	}

	SystemHeapAllocator::Release(fileContent);
}

void Builder::build()
{
#if 0
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
#endif

	for (Shader& shader : shadersList)
	{
		const BindingLayout& bindingLayout = bindingLayoutsList.getEntry(shader.getBindingLayout());
		CompileShader(Platform::D3D12, shader.getType(), bindingLayout.getCompiled(), ...);
	}
}

void Builder::storePackage(const char* packagePath)
{
	// NOTE: Binary blobs in package should have deterministic order

	struct PipelineBlobsDeduplicationListItem
	{
		BinaryBlob* blob;
		uint32 globalPipelineBlobIndex;
	};

	ArrayList<PipelineBlobsDeduplicationListItem, uint32, false> pipelineBlobsDeduplicationList;

	uint32 globalPipelineBlobIndex = 0;
	for (const Pipeline& pipeline : pipelinesList)
	{
		for (BinaryBlob* blob : pipeline.getCompiledBlobs())
		{
			PipelineBlobsDeduplicationListItem item = {};
			item.blob = blob;
			item.globalPipelineBlobIndex = globalPipelineBlobIndex;
			pipelineBlobsDeduplicationList.pushBack(item);

			globalPipelineBlobIndex++;
		}
	}

	pipelineBlobsDeduplicationList.compact();

	SortStable(pipelineBlobsDeduplicationList, ...);

	ArrayList<uint32, uint32, false> pipelinesToCompactedBlobsMap;
	pipelinesToCompactedBlobsMap.resize(pipelineBlobsDeduplicationList.getSize());

	for (uint32 i = 0; i < pipelinesToCompactedBlobsMap.getSize(); i++)
		pipelinesToCompactedBlobsMap[i] = i;




	BinaryBlob* prevBlobsCompactionListItemBlobPtr = nullptr;
	uint32 pipelineBlobsTotalCountAccumulator = 0;
	for (PipelineBlobsCompactionListItem& blobsCompactionListItem : pipelineBlobsCompactionList)
	{
		if (prevBlobsCompactionListItemBlobPtr != blobsCompactionListItem.blob)
			pipelineBlobsTotalCountAccumulator++;
		blobsCompactionListItem.globalSerializedPipelineBlobIndex = pipelineBlobsTotalCountAccumulator;
	}

	// Remove duplicates from `pipelineBlobsCompactionList` ...

	ArrayList<PackFile::BinaryBlobDesc> serializedBinaryBlobs;

	uint32 blobsTotalSizeAccumulator = 0;

	for (const BindingLayout& bindingLayout : bindingLayoutsList)
	{
		const CompiledBindingLayout& compiledLayout = bindingLayout.getCompiled();

		PackFile::BinaryBlobDesc desc = {};
		desc.offset = blobsTotalSizeAccumulator;
		desc.size = compiledLayout.getBinaryBlobSize();
		serializedBinaryBlobs.pushBack(desc);

		blobsTotalSizeAccumulator += compiledLayout.getBinaryBlobSize();
	}

	for (const PipelineBlobOrderedRecord& pipelineRecord : pipelineBlobsCompactionList)
	{

	}

	// ...

	File file;
	file.open(packagePath, FileAccessMode::Write, FileOpenMode::Override);

	PackFile::Header header = {};
	header.signature = PackFile::Signature;
	header.version = PackFile::CurrentVersion;
	header.platformFlags = 0;
	header.bindingLayoutCount = 0;
	header.pipelineCount = 0;
	header.binaryBlobCount = 0;
	file.write(header);

	file.close();
}
