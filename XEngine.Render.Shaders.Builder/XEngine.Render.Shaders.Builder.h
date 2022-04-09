#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include "XEngine.Render.Shaders.Builder.PipelineLayoutList.h"
#include "XEngine.Render.Shaders.Builder.PipelineList.h"
#include "XEngine.Render.Shaders.Builder.ShaderList.h"
#include "XEngine.Render.Shaders.Builder.SourceCache.h"

namespace XEngine::Render::Shaders
{
	class Builder : public XLib::NonCopyable
	{
	private:
		Builder_::PipelineLayoutList pipelineLayoutList;
		Builder_::PipelineList pipelineList;
		Builder_::ShaderList shaderList;
		Builder_::SourceCache sourceCache;

	public:
		Builder() = default;
		~Builder() = default;

		void run(const char* packPath);
	};
}
