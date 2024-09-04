#include <XLib.Math.Matrix4x4.h>
#include <XEngine.Gfx.ShaderLibraryLoader.h>
#include <XEngine.XStringHash.h>

#include "XEngine.Render.SceneRenderer.h"

using namespace XEngine::Gfx;
using namespace XEngine::Render;

struct ViewConstantBuffer
{
	XLib::Matrix4x4 localToWorldSpaceTransform;
	XLib::Matrix4x4 worldToViewTransform;
	XLib::Matrix4x4 worldToClipTransform;
};

struct SceneRenderer::SceneGeometryPassParams
{
	const SceneRenderer* sceneRenderer;
	const Scene* scene;
	CameraDesc cameraDesc;
	uint16 targetWidth;
	uint16 targetHeight;

	Scheduler::TextureHandle gfxSchTargetTexture;
	Scheduler::TextureHandle gfxSchDepthTexture;
};

void SceneRenderer::sceneGeometryPassExecutor(Scheduler::TaskExecutionContext& gfxSchExecutionContext,
	HAL::Device& gfxHwDevice, HAL::CommandList& gfxHwCommandList, SceneGeometryPassParams& params) const
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
	gfxHwCommandList.setPipelineLayout(gfxHwSceneGeometryPipelineLayout);
	gfxHwCommandList.setGraphicsPipeline(gfxHwSceneGeometryPipeline);

	gfxHwCommandList.bindIndexBuffer(Gfx::HAL::BufferPointer { params.scene->gfxHwTestModel, 4096 }, Gfx::HAL::IndexBufferFormat::U16, 4096);
	gfxHwCommandList.bindVertexBuffer(0, Gfx::HAL::BufferPointer { params.scene->gfxHwTestModel, 0 }, 32, 4096);

	static float aaa = 0.0f;

	{
		const XLib::Matrix4x4 cameraViewMatrix = XLib::Matrix4x4::LookAtCentered(params.cameraDesc.position, params.cameraDesc.direction, params.cameraDesc.up);
		const XLib::Matrix4x4 cameraProjectionMatrix = XLib::Matrix4x4::Perspective(params.cameraDesc.fov, float32(params.targetWidth) / float32(params.targetHeight), params.cameraDesc.zNear, params.cameraDesc.zFar);

		const UploadBufferPointer gfxViewCBPointer = gfxSchExecutionContext.allocateTransientUploadMemory(sizeof(ViewConstantBuffer));
		ViewConstantBuffer* viewCB = (ViewConstantBuffer*)gfxViewCBPointer.ptr;
		viewCB->localToWorldSpaceTransform = XLib::Matrix4x4::RotationZ(aaa); aaa += 0.01f;
		viewCB->worldToViewTransform = cameraViewMatrix;
		viewCB->worldToClipTransform = cameraViewMatrix * cameraProjectionMatrix;

		gfxHwCommandList.bindConstantBuffer("view_constant_buffer"_xsh, gfxViewCBPointer.hwPtr);
	}

	gfxHwCommandList.drawIndexed(36);
}

void SceneRenderer::initialize(HAL::Device& gfxHwDevice)
{
	gfxHwSceneGeometryPipelineLayout = GlobalShaderLibraryLoader.getPipelineLayout("SceneGeometry.PipelineLayout"_xsh);

	{
		Gfx::HAL::VertexAttribute attributes[] =
		{
			{ "POSITION",	0,	Gfx::HAL::TexelViewFormat::R32G32B32_FLOAT,	},
			{ "NORMAL",		12,	Gfx::HAL::TexelViewFormat::R32G32B32_FLOAT,	},
			{ "UV",			24,	Gfx::HAL::TexelViewFormat::R32G32_FLOAT,	},
		};

		Gfx::HAL::GraphicsPipelineDesc gfxTestGfxPipelineDesc = {};
		gfxTestGfxPipelineDesc.vertexAttributes = attributes;
		gfxTestGfxPipelineDesc.vertexAttributeCount = countof(attributes);
		gfxTestGfxPipelineDesc.vsHandle = GlobalShaderLibraryLoader.getShader("SceneGeometry.DefaultVS"_xsh);
		gfxTestGfxPipelineDesc.psHandle = GlobalShaderLibraryLoader.getShader("SceneGeometry.DefaultPS"_xsh);
		gfxTestGfxPipelineDesc.colorRenderTargetFormats[0] = Gfx::HAL::TexelViewFormat::R8G8B8A8_UNORM;
		gfxTestGfxPipelineDesc.depthStencilRenderTargetFormat = Gfx::HAL::DepthStencilFormat::D32;
		gfxTestGfxPipelineDesc.depthStencilState.depthReadEnable = true;
		gfxTestGfxPipelineDesc.depthStencilState.depthWriteEnable = true;
		gfxTestGfxPipelineDesc.depthStencilState.depthComparisonFunc = Gfx::HAL::ComparisonFunc::Less;

		gfxHwSceneGeometryPipeline = gfxHwDevice.createGraphicsPipeline(gfxHwSceneGeometryPipelineLayout, gfxTestGfxPipelineDesc);
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

	/*const HAL::TextureDesc gfxHwGBufferATextureDesc =
		HAL::TextureDesc::Create2DRT(targetWidth, targetHeight, HAL::TextureFormat::R8G8B8A8, 1);
	const HAL::TextureDesc gfxHwGBufferBTextureDesc =
		HAL::TextureDesc::Create2DRT(targetWidth, targetHeight, HAL::TextureFormat::R16G16, 1);

	const Scheduler::TextureHandle gfxSchGBufferATexture =
		gfxSchTaskGraph.createTransientTexture(gfxHwGBufferATextureDesc, "GBufferA"_xsh);
	const Scheduler::TextureHandle gfxSchGBufferBTexture =
		gfxSchTaskGraph.createTransientTexture(gfxHwGBufferBTextureDesc, "GBufferB"_xsh);*/

	SceneGeometryPassParams* sceneGeometryPassParams = (SceneGeometryPassParams*)gfxSchTaskGraph.allocateUserData(sizeof(SceneGeometryPassParams));
	*sceneGeometryPassParams = {};
	sceneGeometryPassParams->sceneRenderer = this;
	sceneGeometryPassParams->scene = &scene;
	sceneGeometryPassParams->cameraDesc = cameraDesc;
	sceneGeometryPassParams->targetWidth = targetWidth;
	sceneGeometryPassParams->targetHeight = targetHeight;
	sceneGeometryPassParams->gfxSchTargetTexture = gfxSchTargetTexture;
	sceneGeometryPassParams->gfxSchDepthTexture = gfxSchDepthTexture;

	auto TestPassExecutor = [](Scheduler::TaskExecutionContext& gfxSchExecutionContext,
		HAL::Device& gfxHwDevice, HAL::CommandList& gfxHwCommandList, void* userData) -> void
	{
		SceneGeometryPassParams& params = *(SceneGeometryPassParams*)userData;
		params.sceneRenderer->sceneGeometryPassExecutor(gfxSchExecutionContext, gfxHwDevice, gfxHwCommandList, params);
	};

	gfxSchTaskGraph.addTask(Scheduler::TaskType::Graphics, TestPassExecutor, sceneGeometryPassParams)
		.addColorRenderTarget(gfxSchTargetTexture)
		.addDepthStencilRenderTarget(gfxSchDepthTexture);
}
