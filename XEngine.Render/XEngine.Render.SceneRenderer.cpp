#include <XLib.Math.Matrix4x4.h>
#include <XEngine.Gfx.ShaderLibraryLoader.h>
#include <XEngine.XStringHash.h>

#include "XEngine.Render.SceneRenderer.h"

using namespace XEngine::Gfx;
using namespace XEngine::Render;

struct TestCB
{
	XLib::Matrix4x4 transform;
	XLib::Matrix4x4 view;
	XLib::Matrix4x4 viewProjection;
};

struct SceneRenderer::TestPassParams
{
	const SceneRenderer* sceneRenderer;
	const Scene* scene;
	CameraDesc cameraDesc;
	uint16 targetWidth;
	uint16 targetHeight;

	Scheduler::TextureHandle gfxSchTargetTexture;
	Scheduler::TextureHandle gfxSchDepthTexture;
};

void SceneRenderer::testPassExecutor(Scheduler::TaskExecutionContext& gfxSchExecutionContext,
	HAL::Device& gfxHwDevice, HAL::CommandList& gfxHwCommandList, TestPassParams& params) const
{
	const HAL::TextureHandle gfxHwTargetTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchTargetTexture);
	const HAL::TextureHandle gfxHwDepthTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchDepthTexture);

	gfxHwCommandList.bindRenderTargets(
		HAL::ColorRenderTarget::Create(gfxHwTargetTexture, HAL::TexelViewFormat::R8G8B8A8_UNORM),
		HAL::DepthStencilRenderTarget::Create(gfxHwDepthTexture));

	float32 clearColor[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	gfxHwCommandList.clearColorRenderTarget(0, clearColor);
	gfxHwCommandList.clearDepthStencilRenderTarget(true, false, 1.0f, 0);

	gfxHwCommandList.setViewport(0.0f, 0.0f, float32(params.targetWidth), float32(params.targetHeight));
	gfxHwCommandList.setScissor(0, 0, params.targetWidth, params.targetHeight);

	gfxHwCommandList.setPipelineType(HAL::PipelineType::Graphics);
	gfxHwCommandList.setPipelineLayout(gfxHwTestPipelineLayout);
	gfxHwCommandList.setGraphicsPipeline(gfxHwTestPipeline);

	gfxHwCommandList.bindIndexBuffer(Gfx::HAL::BufferPointer { params.scene->gfxHwTestModel, 4096 }, Gfx::HAL::IndexBufferFormat::U16, 4096);
	gfxHwCommandList.bindVertexBuffer(0, Gfx::HAL::BufferPointer { params.scene->gfxHwTestModel, 0 }, 32, 4096);

	{
		const XLib::Matrix4x4 cameraViewMatrix = XLib::Matrix4x4::LookAtCentered(params.cameraDesc.position, params.cameraDesc.direction, params.cameraDesc.up);
		const XLib::Matrix4x4 cameraProjectionMatrix = XLib::Matrix4x4::Perspective(params.cameraDesc.fov, float32(params.targetWidth) / float32(params.targetHeight), params.cameraDesc.zNear, params.cameraDesc.zFar);

		const UploadBufferPointer gfxCBPointer = gfxSchExecutionContext.allocateTransientUploadMemory(sizeof(TestCB));
		TestCB* testCB = (TestCB*)gfxCBPointer.ptr;
		testCB->transform = XLib::Matrix4x4::Identity();
		testCB->view = cameraViewMatrix;
		testCB->viewProjection = cameraViewMatrix * cameraProjectionMatrix;

		gfxHwCommandList.bindConstantBuffer("SOME_CONSTANT_BUFFER"_xsh, gfxCBPointer.hwPtr);
	}

	gfxHwCommandList.drawIndexed(36);
}

void SceneRenderer::initialize(HAL::Device& gfxHwDevice)
{
	gfxHwTestPipelineLayout = GlobalShaderLibraryLoader.getPipelineLayout("Test.PipelineLayout"_xsh);

	{
		Gfx::HAL::VertexAttribute attributes[] =
		{
			{ "POSITION",	0,	Gfx::HAL::TexelViewFormat::R32G32B32_FLOAT,	},
			{ "NORMAL",		12,	Gfx::HAL::TexelViewFormat::R32G32B32_FLOAT,	},
			{ "TEXCOORD",	24,	Gfx::HAL::TexelViewFormat::R32G32_FLOAT,	},
		};

		Gfx::HAL::GraphicsPipelineDesc gfxTestGfxPipelineDesc = {};
		gfxTestGfxPipelineDesc.vertexAttributes = attributes;
		gfxTestGfxPipelineDesc.vertexAttributeCount = countof(attributes);
		gfxTestGfxPipelineDesc.vsHandle = GlobalShaderLibraryLoader.getShader("TestVS"_xsh);
		gfxTestGfxPipelineDesc.psHandle = GlobalShaderLibraryLoader.getShader("TestPS"_xsh);
		gfxTestGfxPipelineDesc.colorRenderTargetFormats[0] = Gfx::HAL::TexelViewFormat::R8G8B8A8_UNORM;
		gfxTestGfxPipelineDesc.depthStencilRenderTargetFormat = Gfx::HAL::DepthStencilFormat::D32;
		gfxTestGfxPipelineDesc.depthStencilState.depthReadEnable = true;
		gfxTestGfxPipelineDesc.depthStencilState.depthWriteEnable = true;
		gfxTestGfxPipelineDesc.depthStencilState.depthComparisonFunc = Gfx::HAL::ComparisonFunc::Less;

		gfxHwTestPipeline = gfxHwDevice.createGraphicsPipeline(gfxHwTestPipelineLayout, gfxTestGfxPipelineDesc);
	}
}

void SceneRenderer::render(const Scene& scene, const CameraDesc& cameraDesc,
	Scheduler::TaskGraph& gfxSchTaskGraph, Scheduler::TextureHandle gfxSchTargetTexture,
	uint16 targetWidth, uint16 targetHeight)
{
	const HAL::TextureDesc gfxHwDepthTextureDesc =
		HAL::TextureDesc::Create2DRT(targetWidth, targetHeight, HAL::TextureFormat::D32, 1);
	const Scheduler::TextureHandle gfxSchDepthTexture =
		gfxSchTaskGraph.createTransientTexture(gfxHwDepthTextureDesc, "Depth"_xsh);

	TestPassParams* testPassParams = (TestPassParams*)gfxSchTaskGraph.allocateUserData(sizeof(TestPassParams));
	*testPassParams = {};
	testPassParams->sceneRenderer = this;
	testPassParams->scene = &scene;
	testPassParams->cameraDesc = cameraDesc;
	testPassParams->targetWidth = targetWidth;
	testPassParams->targetHeight = targetHeight;
	testPassParams->gfxSchTargetTexture = gfxSchTargetTexture;
	testPassParams->gfxSchDepthTexture = gfxSchDepthTexture;

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
