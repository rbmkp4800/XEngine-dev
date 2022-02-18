#include <XLib.CRC.h>
#include <XLib.String.h>

#include "XEngine.Render.Shaders.Builder.PipelinesList.h"

using namespace XLib;
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

Pipeline* PipelinesList::createPipelineInternal(StringView name, const PipelineLayout& pipelineLayout,
	const GraphicsPipelineDesc* graphicsPipelineDesc, Shader* computeShader)
{
	XAssert((graphicsPipelineDesc == nullptr) != (computeShader == nullptr));

	const uint64 nameCRC = CRC64::Compute(name.getData(), name.getLength());

	if (entriesSearchTree.find(nameCRC))
		return nullptr; // Duplicate name found or CRC collision (can be handled separately).

	Pipeline& pipeline = entriesStorageList.emplaceBack(pipelineLayout);
	pipeline.name = name;
	pipeline.nameCRC = nameCRC;
	pipeline.graphicsDesc = graphicsPipelineDesc ? *graphicsPipelineDesc : GraphicsPipelineDesc {};
	pipeline.computeShader = computeShader;
	pipeline.isGraphics = (graphicsPipelineDesc != nullptr);

	entriesSearchTree.insert(pipeline);

	return &pipeline;
}
