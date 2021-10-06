#include <XLib.h>
#include <XLib.JSON.h>
#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

#include "XEngine.Render.Shaders.Builder.Files.h"

#include "XEngine.Render.Shaders.PackFile.h"

using namespace XLib;
using namespace XEngine::Render::HAL::ShaderCompiler;
using namespace XEngine::Render::Shaders::Builder;

//#if 0

bool IsWhitespace(char c)
{
	return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

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

bool XEngine::Render::Shaders::Builder::LoadShadersListFile(const char* shadersListFilePath,
	ShadersList& shadersList, BindingLayoutsList& bindingLayoutsList, SourcesCache& sourcesCache)
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

bool XEngine::Render::Shaders::Builder::LoadPrevBuildMetadataFile(const char* metadataFilePath,
	ShadersList& shadersList, SourcesCache& sourcesCache, bool& needToRelinkPack)
{

}

bool XEngine::Render::Shaders::Builder::StoreShaderPackFile(const char* shaderPackFilePath,
	const ShadersList& shadersList)
{
	File file;
	file.open(shaderPackFilePath, FileAccessMode::Write, FileOpenMode::Override);

	PackFile::Header header = {};
	header.signature = PackFile::Signature;
	header.version = PackFile::CurrentVersion;
	header.platformFlags = 0;
	header.rootSignatureCount = 0;
	// ASSERT(shadersList.getSize() < UINT16_MAX);
	header.shaderCount = uint32(shadersList.getSize());

	file.write(header);

	file.close();
}
