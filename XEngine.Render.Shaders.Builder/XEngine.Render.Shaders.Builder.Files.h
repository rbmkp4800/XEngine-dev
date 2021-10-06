#pragma once

#include "XEngine.Render.Shaders.Builder.BindingLayoutsList.h"
#include "XEngine.Render.Shaders.Builder.ShadersList.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

namespace XEngine::Render::Shaders::Builder
{
	bool LoadShadersListFile(const char* shadersListFilePath,
		ShadersList& shadersList, BindingLayoutsList& bindingLayoutsList, SourcesCache& sourcesCache);

	bool LoadPrevBuildMetadataFile(const char* metadataFilePath,
		ShadersList& shadersList, SourcesCache& sourcesCache, bool& needToRelinkPack);
	bool StorePrevBuildMetadataFile(const char* metadataFilePath,
		const ShadersList& shadersList, const SourcesCache& sourcesCache);

	bool StoreObjectFile();

	bool StoreShaderPackFile(const char* shaderPackFilePath, const ShadersList& shadersList);
}
