#pragma once

#include "XEngine.Render.Shaders.Builder.CompilerBase.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

namespace XEngine::Render::Shaders::Builder
{
	class CompilerDXC : public CompilerBase
	{
	public:
		CompilerDXC();
		~CompilerDXC();

		virtual CompilerResult compile(ShaderType type, const char* filename, SourcesCache& sourcesCache) override;
	};
}