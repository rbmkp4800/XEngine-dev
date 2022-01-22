#include <XLib.CRC.h>
#include <XLib.String.h>

#include "XEngine.Render.Shaders.Builder.ShadersList.h"

using namespace XLib;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::Shaders::Builder_;

bool Shader::compile()
{
	StringView source = {};
	if (!mainSource.retrieveText(source))
		return false;

	return ShaderCompiler::Host::CompileShader(ShaderCompiler::Platform::D3D12, pipelineLayout.getCompiled(),
		type, mainSource.getLocalPath(), source.getData(), uint32(source.getLength()), compiledShader);
}

Shader* ShadersList::findOrCreateEntry(ShaderCompiler::ShaderType type,
	SourcesCacheEntry& mainSource, const PipelineLayout& pipelineLayout)
{
	CRC64 configurationCRCAccum;
	configurationCRCAccum.process(type);
	configurationCRCAccum.process(mainSource.getLocalPathCRC());
	configurationCRCAccum.process(pipelineLayout.getNameCRC());

	const uint64 configurationCRC = configurationCRCAccum.getValue();

	const EntriesSearchTree::Iterator existingItemIt = entriesSearchTree.find(configurationCRC);
	if (existingItemIt)
		return existingItemIt;

	Shader& shader = entriesStorageList.emplaceBack(mainSource, pipelineLayout);
	shader.configurationHash = configurationCRC;
	shader.entryPointName = "main";
	shader.type = type;

	entriesSearchTree.insert(shader);

	return &shader;
}
