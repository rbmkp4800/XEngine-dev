#include "XEngine.Render.Shaders.Builder.PipelinesList.h"

using namespace XEngine::Render::HAL;
using namespace XEngine::Render::Shaders::Builder_;

bool Pipeline::compile()
{
	if (!isGraphics)
		return true; // Compute shader already compiled

	ShaderCompiler::GraphicsPipelineDesc desc = {};
	desc.vertexShader = graphicsDesc.vertexShader ? &graphicsDesc.vertexShader->getCompiled() : nullptr;
	desc.amplificationShader = graphicsDesc.amplificationShader ? &graphicsDesc.amplificationShader->getCompiled() : nullptr;
	desc.meshShader = graphicsDesc.meshShader ? &graphicsDesc.meshShader->getCompiled() : nullptr;
	desc.pixelShader = graphicsDesc.pixelShader ? &graphicsDesc.pixelShader->getCompiled() : nullptr;

	for (uint32 i = 0; i < MaxRenderTargetCount; i++)
		desc.renderTargetsFormats[i] = graphicsDesc.renderTargetsFormats[i];
	desc.depthStencilFormat = graphicsDesc.depthStencilFormat;

	return ShaderCompiler::Host::CompileGraphicsPipeline(ShaderCompiler::Platform::D3D12,
		pipelineLayout.getCompiled(), desc, compiledGraphicsPipeline);
}

Pipeline* PipelinesList::createGraphicsPipeline(const char* name, const PipelineLayout& pipelineLayout, const GraphicsPipelineDesc& pipelineDesc)
{

}

Pipeline* PipelinesList::createComputePipeline(const char* name, const PipelineLayout& pipelineLayout, const Shader& computeShader)
{

}
