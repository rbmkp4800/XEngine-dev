#include "XEngine.Render.SceneRenderer.h"

#include <XEngine.Gfx.ShaderLibraryLoader.h>
#include <XEngine.XStringHash.h>

using namespace XEngine::Gfx;
using namespace XEngine::Render;

struct SceneRenderer::TestPassParams
{
	SceneRenderer* sceneRenderer;
	Scheduler::TextureHandle gfxSchTargetTexture;
	Scheduler::TextureHandle gfxSchDepthTexture;
	uint16 targetWidth;
	uint16 targetHeight;
};

void SceneRenderer::testPassExecutor(Scheduler::TaskExecutionContext& gfxSchExecutionContext,
	HAL::Device& gfxHwDevice, HAL::CommandList& gfxHwCommandList, TestPassParams& params)
{
	const HAL::TextureHandle gfxHwTargetTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchTargetTexture);
	const HAL::TextureHandle gfxHwDepthTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchDepthTexture);

	gfxHwCommandList.bindRenderTargets(
		HAL::ColorRenderTarget::Create(gfxHwTargetTexture, HAL::TexelViewFormat::R8G8B8A8_UNORM),
		HAL::DepthStencilRenderTarget::Create(gfxHwDepthTexture));

	float32 clearColor[4] = {};
	gfxHwCommandList.clearColorRenderTarget(0, clearColor);
	gfxHwCommandList.clearDepthStencilRenderTarget(true, false, 0.0f, 0);

	gfxHwCommandList.setViewport(0.0f, 0.0f, float32(params.targetWidth), float32(params.targetHeight));
	gfxHwCommandList.setScissor(0, 0, params.targetWidth, params.targetHeight);

	gfxHwCommandList.setPipelineType(HAL::PipelineType::Graphics);
	gfxHwCommandList.setPipelineLayout(gfxHwTestPipelineLayout);
	gfxHwCommandList.setGraphicsPipeline(gfxHwTestPipeline);
	gfxHwCommandList.draw(3);
}

void SceneRenderer::initialize(HAL::Device& gfxHwDevice)
{
	gfxHwTestPipelineLayout = GlobalShaderLibraryLoader.getPipelineLayout("Test.PipelineLayout"_xsh);

	{
		HAL::GraphicsPipelineDesc gfxHwTestPipelineDesc = {};
		gfxHwTestPipelineDesc.vsHandle = GlobalShaderLibraryLoader.getShader("TestTriangleVS"_xsh);
		gfxHwTestPipelineDesc.psHandle = GlobalShaderLibraryLoader.getShader("TestTrianglePS"_xsh);
		gfxHwTestPipelineDesc.colorRenderTargetFormats[0] = HAL::TexelViewFormat::R8G8B8A8_UNORM;
		gfxHwTestPipeline = gfxHwDevice.createGraphicsPipeline(gfxHwTestPipelineLayout, gfxHwTestPipelineDesc);
	}
}

void SceneRenderer::render(
	Scheduler::TaskGraph& gfxSchTaskGraph,
	Scheduler::TextureHandle gfxSchTargetTexture,
	uint16 targetWidth, uint16 targetHeight)
{
	const HAL::TextureDesc gfxHwDepthTextureDesc =
		HAL::TextureDesc::Create2DRT(targetWidth, targetHeight, HAL::TextureFormat::D32, 1);
	const Scheduler::TextureHandle gfxSchDepthTexture =
		gfxSchTaskGraph.createTransientTexture(gfxHwDepthTextureDesc, "Depth"_xsh);

	TestPassParams* testPassParams = (TestPassParams*)gfxSchTaskGraph.allocateUserData(sizeof(TestPassParams));
	*testPassParams = {};
	testPassParams->sceneRenderer = this;
	testPassParams->gfxSchTargetTexture = gfxSchTargetTexture;
	testPassParams->gfxSchDepthTexture = gfxSchDepthTexture;
	testPassParams->targetWidth = targetWidth;
	testPassParams->targetHeight = targetHeight;

	auto TestPassExecutor = [](Scheduler::TaskExecutionContext& gfxSchExecutionContext,
		HAL::Device& gfxHwDevice, HAL::CommandList& gfxHwCommandList, void* userData) -> void
	{
		TestPassParams& params = *(TestPassParams*)userData;
		params.sceneRenderer->testPassExecutor(gfxSchExecutionContext, gfxHwDevice, gfxHwCommandList, params);
	};

	gfxSchTaskGraph.addTask(Scheduler::TaskType::Graphics, TestPassExecutor, testPassParams)
		.addColorRenderTarget(gfxSchTargetTexture)
		.addDepthStencilRenderTarget(gfxSchDepthTexture);
}
