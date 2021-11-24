#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include "XEngine.Render.Shaders.Builder.PipelineLayoutsList.h"
#include "XEngine.Render.Shaders.Builder.PipelinesList.h"
#include "XEngine.Render.Shaders.Builder.ShadersList.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

namespace XEngine::Render::Shaders
{
	class Builder : public XLib::NonCopyable
	{
	private:
		Builder_::PipelineLayoutsList pipelineLayoutsList;
		Builder_::PipelinesList pipelinesList;
		Builder_::ShadersList shadersList;
		Builder_::SourcesCache sourcesCache;

	public:
		Builder() = default;
		~Builder() = default;

		bool loadIndex(const char* indexPath);

		void build();

		void composePack(const char* packPath);
	};
}
