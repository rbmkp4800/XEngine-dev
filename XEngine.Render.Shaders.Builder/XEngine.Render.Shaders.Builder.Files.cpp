#include <XLib.h>
#include <XLib.JSON.h>
#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>

#include "XEngine.Render.Shaders.Builder.Files.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder;

//#if 0

bool IsWhitespace(char c)
{
	return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

static inline ShaderType ShaderTypeFromString(const StringView& str)
{
	if (str.length != 2 || str.data[1] != 'S')
		return ShaderType::None;

	switch (str.data[0])
	{
		case 'C':	return ShaderType::CS;
		case 'V':	return ShaderType::VS;
		case 'M':	return ShaderType::MS;
		case 'A':	return ShaderType::AS;
		case 'P':	return ShaderType::PS;
	}
	return ShaderType::None;
}

bool XEngine::Render::Shaders::Builder::LoadShadersListFile(const char* shadersListFilePath,
	ShadersList& shadersList, SourcesCache& sourcesCache)
{
	File file;
	file.open(shadersListFilePath, FileAccessMode::Read);
	
	const uint64 fileSize = file.getSize();

	// TODO: This should be some kind of safe ptr, so we can release on return
	char* fileContent = (char*)SystemHeapAllocator::Allocate(fileSize);

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

		const ShaderType shaderType = ShaderTypeFromString(shaderTypeStr);
		if (shaderType == ShaderType::None)
			return false;

		JSONNodeId entryPointNameNodeId = jsonTree.getProperty(jsonRootArrayIt, "entryPoint");
		if (entryPointNameNodeId != JSONInvalidNodeId)
		{
			if (!jsonTree.isString(entryPointNameNodeId))
				return false;
			shaderEntryPoint = jsonTree.asString(entryPointNameNodeId);
		}

		const SourcesCacheEntryId mainSourceId = sourcesCache.findOrCreateEntry(shaderSourcePath);

		ShadersListEntry* shaderEntry = shadersList.createEntry(shaderName, shaderType, mainSourceId);
		if (!shaderEntry)
		{
			// Duplicate shader
			return false;
		}
	}

	SystemHeapAllocator::Release(fileContent);
}

bool XEngine::Render::Shaders::Builder::LoadPrevBuildMetadataFile(const char* metadataFilePath,
	ShadersList& shadersList, SourcesCache& sourcesCache, bool& needToRelinkPack)
{

}
