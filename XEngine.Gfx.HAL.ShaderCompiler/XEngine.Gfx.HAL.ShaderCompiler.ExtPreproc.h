#pragma once

#include <XLib.h>
#include <XLib.String.h>

// TODO: Proper numeric literals lexing support.
// TODO: Add '#line' directives support.
// TODO: Implement `[[attr1]] [[attr2]]` support.

namespace XEngine::Gfx::HAL::ShaderCompiler { class PipelineLayout; }

namespace XEngine::Gfx::HAL::ShaderCompiler::ExtPreproc
{
	bool Preprocess(XLib::StringViewASCII source, XLib::StringViewASCII mainSourceFilename,
		const PipelineLayout& pipelineLayout, XLib::DynamicStringASCII& patchedSource, XLib::VirtualStringRefASCII output);
}
