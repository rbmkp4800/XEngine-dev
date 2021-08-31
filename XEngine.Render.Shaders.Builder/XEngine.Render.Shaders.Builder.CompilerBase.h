#pragma once

#include "XEngine.Render.Shaders.Builder.Common.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

namespace XEngine::Render::Shaders::Builder
{
	struct CompilerResult
	{
		
	};

	class CompilerBase abstract
	{
	public:
		virtual CompilerResult compile(ShaderType type, SourcesCacheEntryId sourceId, SourcesCache& sourcesCache) = 0;
	};
}
