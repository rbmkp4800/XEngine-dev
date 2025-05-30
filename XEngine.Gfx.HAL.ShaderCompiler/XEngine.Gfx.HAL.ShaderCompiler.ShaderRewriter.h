#pragma once

#include <XLib.h>
#include <XLib.String.h>

// TODO: Proper numeric literals lexing support.
// TODO: Add '#line' directives support.
// TODO: Implement `[[attr1]] [[attr2]]` support.

namespace XEngine::Gfx::HAL::ShaderCompiler { class PipelineLayout; }

namespace XEngine::Gfx::HAL::ShaderCompiler::ShaderRewriter
{
	bool Rewrite(XLib::StringViewASCII source, XLib::StringViewASCII mainSourceFilename,
		const PipelineLayout& pipelineLayout, XLib::DynamicStringASCII& rewrittenSource, XLib::VirtualStringRefASCII output);
}
