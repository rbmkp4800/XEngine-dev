#pragma once

#include <XLib.Platform.COMPtr.h>

#include "XEngine.Render.Shaders.Builder.CompilerBase.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

struct IDxcCompiler3;

namespace XEngine::Render::Shaders::Builder
{
	class CompilerDXC : public CompilerBase
	{
	private:
		XLib::Platform::COMPtr<IDxcCompiler3> dxcCompiler;

	public:
		CompilerDXC();
		~CompilerDXC();

		virtual CompilerResult compile(ShaderType type, SourcesCacheEntryId sourceId, SourcesCache& sourcesCache) override;
	};
}
