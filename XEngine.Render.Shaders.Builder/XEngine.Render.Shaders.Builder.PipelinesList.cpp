#include "XEngine.Render.Shaders.Builder.PipelinesList.h"

using namespace XEngine::Render::HAL;
using namespace XEngine::Render::HAL::ShaderCompiler;
using namespace XEngine::Render::Shaders::Builder_;

bool PipelineLayout::compile()
{
	PipelineBindPointDesc bindPointDescs[MaxPipelineBindPointCount] = {};
	for (uint8 i = 0; i < bindPoints.getSize(); i++)
	{
		const BindPoint& src = bindPoints[i];
		PipelineBindPointDesc& dst = bindPointDescs[i];
		dst.name = src.name.getCStr();
		dst.type = src.type;
		dst.shaderVisibility = src.shaderVisibility;
		dst.constantsSize32bitValues = src.constantsSize32bitValues;
	}

	return Host::CompilePipelineLayout(Platform::D3D12,
		PipelineLayoutDesc{ bindPointDescs, bindPoints.getSize() }, compiledPipelineLayout);
}
