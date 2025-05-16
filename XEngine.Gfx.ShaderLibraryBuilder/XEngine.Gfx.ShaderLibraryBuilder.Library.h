#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>

#include <XEngine.Gfx.HAL.ShaderCompiler.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.Shader.h"

namespace XEngine::Gfx::ShaderLibraryBuilder
{
	struct Library
	{
		struct DescriptorSetLayout
		{
			uint64 nameXSH;
			HAL::ShaderCompiler::DescriptorSetLayoutRef ref;
		};

		struct PipelineLayout
		{
			uint64 nameXSH;
			HAL::ShaderCompiler::PipelineLayoutRef ref;
		};

		XLib::ArrayList<DescriptorSetLayout> descriptorSetLayouts;
		XLib::ArrayList<PipelineLayout> pipelineLayouts;
		XLib::ArrayList<ShaderRef> shaders;
	};
}
