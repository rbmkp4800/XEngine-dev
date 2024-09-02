#include "XEngine.Render.SceneRenderer.h"

#include <XEngine.Gfx.ShaderLibraryLoader.h>
#include <XEngine.XStringHash.h>

using namespace XEngine::Render;

void SceneRenderer::testPassExecutor(Gfx::Scheduler::TaskExecutionContext& gfxSchExecutionContext,
	Gfx::HAL::Device& gfxHwDevice, Gfx::HAL::CommandList& gfxHwCommandList, TestPassUserData& userData)
{
	const Gfx::HAL::TextureHandle gfxHwTargetTexture = gfxSchExecutionContext.resolveTexture(userData.gfxSchTargetTexture);

	struct Gfx::HAL::ColorRenderTarget gfxHwRT = {};
	gfxHwRT.texture = gfxHwTargetTexture;
	gfxHwRT.format = Gfx::HAL::TexelViewFormat::R8G8B8A8_UNORM;

	gfxHwCommandList.bindRenderTargets(1, &gfxHwRT);

	float32 clearColor[4] = {};
	gfxHwCommandList.clearColorRenderTarget(0, clearColor);

	gfxHwCommandList.setViewport(0.0f, 0.0f, 1280.0f, 720.0f);
	gfxHwCommandList.setScissor(0, 0, 1280, 720);

	gfxHwCommandList.setPipelineType(Gfx::HAL::PipelineType::Graphics);
	gfxHwCommandList.setPipelineLayout(gfxHwTestPipelineLayout);
	gfxHwCommandList.setGraphicsPipeline(gfxHwTestPipeline);
	gfxHwCommandList.draw(3);
}

void SceneRenderer::initialize(Gfx::HAL::Device& gfxHwDevice)
{
	gfxHwTestPipelineLayout = Gfx::GlobalShaderLibraryLoader.getPipelineLayout("Test.PipelineLayout"_xsh);

	{
		Gfx::HAL::GraphicsPipelineDesc gfxHwTestPipelineDesc = {};
		gfxHwTestPipelineDesc.vsHandle = Gfx::GlobalShaderLibraryLoader.getShader("TestTriangleVS"_xsh);
		gfxHwTestPipelineDesc.psHandle = Gfx::GlobalShaderLibraryLoader.getShader("TestTrianglePS"_xsh);
		gfxHwTestPipelineDesc.colorRenderTargetFormats[0] = Gfx::HAL::TexelViewFormat::R8G8B8A8_UNORM;
		gfxHwTestPipeline = gfxHwDevice.createGraphicsPipeline(gfxHwTestPipelineLayout, gfxHwTestPipelineDesc);
	}
}

void SceneRenderer::render(
	Gfx::Scheduler::TaskGraph& gfxSchTaskGraph,
	Gfx::Scheduler::TextureHandle gfxSchTargetTexture)
{
	TestPassUserData* testPassUserData = (TestPassUserData*)gfxSchTaskGraph.allocateUserData(sizeof(TestPassUserData));
	*testPassUserData = {};
	testPassUserData->sceneRenderer = this;

	auto TestPassExecutor = [](Gfx::Scheduler::TaskExecutionContext& gfxSchExecutionContext,
		Gfx::HAL::Device& gfxHwDevice, Gfx::HAL::CommandList& gfxHwCommandList, void* userData) -> void
	{
		TestPassUserData& typedUserData = *(TestPassUserData*)userData;
		typedUserData.sceneRenderer->testPassExecutor(gfxSchExecutionContext, gfxHwDevice, gfxHwCommandList, typedUserData);
	};

	gfxSchTaskGraph.addTask(Gfx::Scheduler::TaskType::Graphics, TestPassExecutor, testPassUserData)
		.addColorRenderTarget(gfxSchTargetTexture);
}
