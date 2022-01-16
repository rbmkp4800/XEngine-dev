#include "XEngine.Render.Shaders.Builder.ShadersList.h"

using namespace XLib;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::Shaders::Builder_;

bool Shader::compile()
{
	StringView source = {};
	if (!mainSource.retrieveText(source))
		return false;

	return ShaderCompiler::Host::CompileShader(ShaderCompiler::Platform::D3D12,
		pipelineLayout.getCompiled(), type, source.getData(), uint32(source.getLength()), compiledShader);
}

Shader* ShadersList::findOrCreateEntry(ShaderCompiler::ShaderType type,
	SourcesCacheEntry& mainSource, const PipelineLayout& pipelineLayout)
{

}
