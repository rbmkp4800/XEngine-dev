#include <XLib.CRC.h>
#include <XLib.String.h>

#include "XEngine.Render.Shaders.Builder.PipelineList.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder_;

bool Pipeline::compile()
{
	if (!isGraphics)
		return true; // Compute shader already compiled

	HAL::ShaderCompiler::GraphicsPipelineDesc desc = {};
	desc.vertexShader = graphicsDesc.vertexShader ? &graphicsDesc.vertexShader->getCompiled() : nullptr;
	desc.amplificationShader = graphicsDesc.amplificationShader ? &graphicsDesc.amplificationShader->getCompiled() : nullptr;
	desc.meshShader = graphicsDesc.meshShader ? &graphicsDesc.meshShader->getCompiled() : nullptr;
	desc.pixelShader = graphicsDesc.pixelShader ? &graphicsDesc.pixelShader->getCompiled() : nullptr;

	for (uint32 i = 0; i < HAL::MaxRenderTargetCount; i++)
		desc.renderTargetsFormats[i] = graphicsDesc.renderTargetsFormats[i];
	desc.depthStencilFormat = graphicsDesc.depthStencilFormat;

	return HAL::ShaderCompiler::Host::CompileGraphicsPipeline(HAL::ShaderCompiler::Platform::D3D12,
		pipelineLayout->getCompiled(), desc, compiledGraphicsPipeline);
}

PipelineCreationResult PipelineList::create(StringViewASCII name, const PipelineLayout& pipelineLayout,
	const GraphicsPipelineDesc* graphicsPipelineDesc, Shader* computeShader)
{
	XAssert((graphicsPipelineDesc == nullptr) != (computeShader == nullptr));

	const uint64 nameCRC = CRC64::Compute(name.getData(), name.getLength());
	if (entrySearchTree.find(nameCRC))
	{
		// Duplicate name found or CRC collision. These cases should be handled separately.
		return PipelineCreationResult { PipelineCreationStatus::Failure_EntryNameDuplication };
	}

	const uint32 memoryBlockSize = sizeof(Pipeline) + uint32(name.getLength()) + 1;
	byte* pipelineMemory = (byte*)SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(pipelineMemory, 0, memoryBlockSize);

	char* nameMemory = (char*)(pipelineMemory + sizeof(Pipeline));
	memoryCopy(nameMemory, name.getData(), name.getLength());
	nameMemory[name.getLength()] = '\0';

	Pipeline& pipeline = *(Pipeline*)pipelineMemory;
	construct(pipeline);
	pipeline.name = StringView(nameMemory, name.getLength());
	pipeline.nameCRC = nameCRC;
	pipeline.pipelineLayout = &pipelineLayout;
	pipeline.graphicsDesc = graphicsPipelineDesc ? *graphicsPipelineDesc : GraphicsPipelineDesc {};
	pipeline.computeShader = computeShader;
	pipeline.isGraphics = (graphicsPipelineDesc != nullptr);

	entrySearchTree.insert(pipeline);
	entryCount++;

	return PipelineCreationResult { PipelineCreationStatus::Success, &pipeline };
}
