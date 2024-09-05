#include <XLib.Math.Matrix4x4.h>
#include <XEngine.Gfx.ShaderLibraryLoader.h>
#include <XEngine.XStringHash.h>

#include "XEngine.Render.SceneRenderer.h"

using namespace XEngine::Gfx;
using namespace XEngine::Render;

struct ViewConstantBuffer
{
	XLib::Matrix4x4 worldToViewTransform;
	XLib::Matrix4x4 worldToClipTransform;
	XLib::Matrix4x4 viewToWorldTransform;
	float32x4 ndcDeprojectionCoefs;
};

struct DeferredLightingConstantBuffer
{
	float32x3 lightDirection;
};

struct TonemappingConstantBuffer
{
	float32 exposure;
	uint32 _padding[3];
};

struct SceneRenderer::CommonParams
{
	const SceneRenderer* sceneRenderer;
	const Scene* scene;
	uint16 targetWidth;
	uint16 targetHeight;

	HAL::BufferPointer gfxHwViewConstantBufferPtr;
	HAL::BufferPointer gfxHwDeferredLightingConstantBufferPtr;
	HAL::BufferPointer gfxHwTonemappingConstantBufferPtr;
	Scheduler::TextureHandle gfxSchDepthTexture;
	Scheduler::TextureHandle gfxSchGBufferATexture;
	Scheduler::TextureHandle gfxSchGBufferBTexture;
	Scheduler::TextureHandle gfxSchGBufferCTexture;
	Scheduler::TextureHandle gfxSchLuminanceTexture;
	Scheduler::TextureHandle gfxSchTargetTexture;
};

void SceneRenderer::sceneGeometryPassExecutor(Scheduler::TaskExecutionContext& gfxSchExecutionContext,
	HAL::Device& gfxHwDevice, HAL::CommandList& gfxHwCommandList, CommonParams& params) const
{
	const HAL::TextureHandle gfxHwGBufferATexture = gfxSchExecutionContext.resolveTexture(params.gfxSchGBufferATexture);
	const HAL::TextureHandle gfxHwGBufferBTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchGBufferBTexture);
	const HAL::TextureHandle gfxHwGBufferCTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchGBufferCTexture);
	const HAL::TextureHandle gfxHwDepthTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchDepthTexture);

	const HAL::ColorRenderTarget gfxHwColorRTs[] =
	{
		HAL::ColorRenderTarget::Create(gfxHwGBufferATexture, HAL::TexelViewFormat::R8G8B8A8_UNORM),
		HAL::ColorRenderTarget::Create(gfxHwGBufferBTexture, HAL::TexelViewFormat::R16G16B16A16_FLOAT),
		HAL::ColorRenderTarget::Create(gfxHwGBufferCTexture, HAL::TexelViewFormat::R8G8_UNORM),
	};
	const HAL::DepthStencilRenderTarget gfxHwDepthRT = HAL::DepthStencilRenderTarget::Create(gfxHwDepthTexture);

	gfxHwCommandList.bindRenderTargets(countOf(gfxHwColorRTs), gfxHwColorRTs, &gfxHwDepthRT);

	float32 clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	// TODO: GBuffer clears should be replaced with `initializeTextureMetadata`.
	gfxHwCommandList.clearColorRenderTarget(0, clearColor);
	gfxHwCommandList.clearColorRenderTarget(1, clearColor);
	gfxHwCommandList.clearColorRenderTarget(2, clearColor);

	gfxHwCommandList.clearDepthStencilRenderTarget(true, false, 1.0f, 0);

	gfxHwCommandList.setViewport(0.0f, 0.0f, float32(params.targetWidth), float32(params.targetHeight));
	gfxHwCommandList.setScissor(0, 0, params.targetWidth, params.targetHeight);

	gfxHwCommandList.setPipelineType(HAL::PipelineType::Graphics);
	gfxHwCommandList.setPipelineLayout(gfxHwSceneGeometryPipelineLayout);
	gfxHwCommandList.setGraphicsPipeline(gfxHwSceneGeometryPipeline);

	gfxHwCommandList.bindIndexBuffer(Gfx::HAL::BufferPointer { params.scene->gfxHwTestModel, 4096 }, Gfx::HAL::IndexBufferFormat::U16, 4096);
	gfxHwCommandList.bindVertexBuffer(0, Gfx::HAL::BufferPointer { params.scene->gfxHwTestModel, 0 }, 44, 4096);
	gfxHwCommandList.bindConstantBuffer("view_constant_buffer"_xsh, params.gfxHwViewConstantBufferPtr);

	gfxHwCommandList.drawIndexed(36);
}

void SceneRenderer::deferredLightingPassExecutor(Gfx::Scheduler::TaskExecutionContext& gfxSchExecutionContext,
	Gfx::HAL::Device& gfxHwDevice, Gfx::HAL::CommandList& gfxHwCommandList, CommonParams& params) const
{
	const HAL::TextureHandle gfxHwDepthTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchDepthTexture);
	const HAL::TextureHandle gfxHwGBufferATexture = gfxSchExecutionContext.resolveTexture(params.gfxSchGBufferATexture);
	const HAL::TextureHandle gfxHwGBufferBTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchGBufferBTexture);
	const HAL::TextureHandle gfxHwGBufferCTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchGBufferCTexture);
	const HAL::TextureHandle gfxHwLuminanceTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchLuminanceTexture);

	const HAL::DescriptorSet gfxHwGBufferTexturesDS = gfxSchExecutionContext.allocateTransientDescriptorSet(gfxHwGBufferTexturesDSL);
	gfxHwDevice.writeDescriptorSet(gfxHwGBufferTexturesDS, "depth"_xsh,
		HAL::ResourceView::CreateTexture2D(gfxHwDepthTexture, HAL::TexelViewFormat::R32_FLOAT));
	gfxHwDevice.writeDescriptorSet(gfxHwGBufferTexturesDS, "gbuffer_a"_xsh,
		HAL::ResourceView::CreateTexture2D(gfxHwGBufferATexture, HAL::TexelViewFormat::R8G8B8A8_SRGB));
	gfxHwDevice.writeDescriptorSet(gfxHwGBufferTexturesDS, "gbuffer_b"_xsh,
		HAL::ResourceView::CreateTexture2D(gfxHwGBufferBTexture, HAL::TexelViewFormat::R16G16B16A16_FLOAT));
	gfxHwDevice.writeDescriptorSet(gfxHwGBufferTexturesDS, "gbuffer_c"_xsh,
		HAL::ResourceView::CreateTexture2D(gfxHwGBufferCTexture, HAL::TexelViewFormat::R8G8_UNORM));

	gfxHwCommandList.bindRenderTargets(HAL::ColorRenderTarget::Create(gfxHwLuminanceTexture, HAL::TexelViewFormat::R16G16B16A16_FLOAT));

	gfxHwCommandList.setViewport(0.0f, 0.0f, float32(params.targetWidth), float32(params.targetHeight));
	gfxHwCommandList.setScissor(0, 0, params.targetWidth, params.targetHeight);

	gfxHwCommandList.setPipelineType(HAL::PipelineType::Graphics);
	gfxHwCommandList.setPipelineLayout(gfxHwDeferredLightingPipelineLayout);
	gfxHwCommandList.setGraphicsPipeline(gfxHwDeferredLightingPipeline);

	gfxHwCommandList.bindConstantBuffer("view_constant_buffer"_xsh, params.gfxHwViewConstantBufferPtr);
	gfxHwCommandList.bindConstantBuffer("deferrred_lighting_constant_buffer"_xsh, params.gfxHwDeferredLightingConstantBufferPtr);
	gfxHwCommandList.bindDescriptorSet("gbuffer_descriptors"_xsh, gfxHwGBufferTexturesDS);

	gfxHwCommandList.draw(3);
}

void SceneRenderer::tonemappingPassExecutor(Gfx::Scheduler::TaskExecutionContext& gfxSchExecutionContext,
	Gfx::HAL::Device& gfxHwDevice, Gfx::HAL::CommandList& gfxHwCommandList, CommonParams& params) const
{
	const HAL::TextureHandle gfxHwLuminanceTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchLuminanceTexture);
	const HAL::TextureHandle gfxHwTargetTexture = gfxSchExecutionContext.resolveTexture(params.gfxSchTargetTexture);

	const HAL::DescriptorSet gfxHwTonemappingInputDS = gfxSchExecutionContext.allocateTransientDescriptorSet(gfxHwTonemappingInputDSL);
	gfxHwDevice.writeDescriptorSet(gfxHwTonemappingInputDS, "luminance"_xsh,
		HAL::ResourceView::CreateTexture2D(gfxHwLuminanceTexture, HAL::TexelViewFormat::R16G16B16A16_FLOAT));

	gfxHwCommandList.bindRenderTargets(HAL::ColorRenderTarget::Create(gfxHwTargetTexture, HAL::TexelViewFormat::R8G8B8A8_UNORM));

	gfxHwCommandList.setViewport(0.0f, 0.0f, float32(params.targetWidth), float32(params.targetHeight));
	gfxHwCommandList.setScissor(0, 0, params.targetWidth, params.targetHeight);

	gfxHwCommandList.setPipelineType(HAL::PipelineType::Graphics);
	gfxHwCommandList.setPipelineLayout(gfxHwTonemappingPipelineLayout);
	gfxHwCommandList.setGraphicsPipeline(gfxHwTonemappingPipeline);

	gfxHwCommandList.bindConstantBuffer("tonemapping_constant_buffer"_xsh, params.gfxHwTonemappingConstantBufferPtr);
	gfxHwCommandList.bindDescriptorSet("tonemapping_input_descriptors"_xsh, gfxHwTonemappingInputDS);

	gfxHwCommandList.draw(3);
}

void SceneRenderer::initialize(HAL::Device& gfxHwDevice)
{
	gfxHwGBufferTexturesDSL = GlobalShaderLibraryLoader.getDescriptorSetLayout("GBufferTexturesDSL"_xsh);
	gfxHwTonemappingInputDSL = GlobalShaderLibraryLoader.getDescriptorSetLayout("Tonemapping.InputDSL"_xsh);

	gfxHwSceneGeometryPipelineLayout = GlobalShaderLibraryLoader.getPipelineLayout("SceneGeometry.PipelineLayout"_xsh);
	gfxHwDeferredLightingPipelineLayout = GlobalShaderLibraryLoader.getPipelineLayout("DeferredLighting.PipelineLayout"_xsh);
	gfxHwTonemappingPipelineLayout = GlobalShaderLibraryLoader.getPipelineLayout("Tonemapping.PipelineLayout"_xsh);

	{
		Gfx::HAL::VertexAttribute attributes[] =
		{
			{ "POSITION",	0,	Gfx::HAL::TexelViewFormat::R32G32B32_FLOAT,	},
			{ "NORMAL",		12,	Gfx::HAL::TexelViewFormat::R32G32B32_FLOAT,	},
			{ "TANGENT",	24,	Gfx::HAL::TexelViewFormat::R32G32B32_FLOAT,	},
			{ "UV",			36,	Gfx::HAL::TexelViewFormat::R32G32_FLOAT,	},
		};

		Gfx::HAL::GraphicsPipelineDesc gfxHwPipelineDesc = {};
		gfxHwPipelineDesc.vertexAttributes = attributes;
		gfxHwPipelineDesc.vertexAttributeCount = countOf(attributes);
		gfxHwPipelineDesc.vsHandle = GlobalShaderLibraryLoader.getShader("SceneGeometry.DefaultVS"_xsh);
		gfxHwPipelineDesc.psHandle = GlobalShaderLibraryLoader.getShader("SceneGeometry.DefaultPS"_xsh);
		gfxHwPipelineDesc.colorRenderTargetFormats[0] = Gfx::HAL::TexelViewFormat::R8G8B8A8_UNORM;
		gfxHwPipelineDesc.colorRenderTargetFormats[1] = Gfx::HAL::TexelViewFormat::R16G16B16A16_FLOAT;
		gfxHwPipelineDesc.colorRenderTargetFormats[2] = Gfx::HAL::TexelViewFormat::R8G8_UNORM;
		gfxHwPipelineDesc.depthStencilRenderTargetFormat = Gfx::HAL::DepthStencilFormat::D32;
		gfxHwPipelineDesc.depthStencilState.depthReadEnable = true;
		gfxHwPipelineDesc.depthStencilState.depthWriteEnable = true;
		gfxHwPipelineDesc.depthStencilState.depthComparisonFunc = Gfx::HAL::ComparisonFunc::Less;

		gfxHwSceneGeometryPipeline = gfxHwDevice.createGraphicsPipeline(gfxHwSceneGeometryPipelineLayout, gfxHwPipelineDesc);
	}

	{
		Gfx::HAL::GraphicsPipelineDesc gfxHwPipelineDesc = {};
		gfxHwPipelineDesc.vsHandle = GlobalShaderLibraryLoader.getShader("DeferredLighting.VS"_xsh);
		gfxHwPipelineDesc.psHandle = GlobalShaderLibraryLoader.getShader("DeferredLighting.PS"_xsh);
		gfxHwPipelineDesc.colorRenderTargetFormats[0] = Gfx::HAL::TexelViewFormat::R16G16B16A16_FLOAT;

		gfxHwDeferredLightingPipeline = gfxHwDevice.createGraphicsPipeline(gfxHwDeferredLightingPipelineLayout, gfxHwPipelineDesc);
	}

	{
		Gfx::HAL::GraphicsPipelineDesc gfxHwPipelineDesc = {};
		gfxHwPipelineDesc.vsHandle = GlobalShaderLibraryLoader.getShader("Tonemapping.VS"_xsh);
		gfxHwPipelineDesc.psHandle = GlobalShaderLibraryLoader.getShader("Tonemapping.PS"_xsh);
		gfxHwPipelineDesc.colorRenderTargetFormats[0] = Gfx::HAL::TexelViewFormat::R8G8B8A8_UNORM;

		gfxHwTonemappingPipeline = gfxHwDevice.createGraphicsPipeline(gfxHwTonemappingPipelineLayout, gfxHwPipelineDesc);
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

	const HAL::TextureDesc gfxHwGBufferATextureDesc =
		HAL::TextureDesc::Create2DRT(targetWidth, targetHeight, HAL::TextureFormat::R8G8B8A8, 1);
	const HAL::TextureDesc gfxHwGBufferBTextureDesc =
		HAL::TextureDesc::Create2DRT(targetWidth, targetHeight, HAL::TextureFormat::R16G16B16A16, 1);
	const HAL::TextureDesc gfxHwGBufferCTextureDesc =
		HAL::TextureDesc::Create2DRT(targetWidth, targetHeight, HAL::TextureFormat::R8G8, 1);

	const Scheduler::TextureHandle gfxSchGBufferATexture =
		gfxSchTaskGraph.createTransientTexture(gfxHwGBufferATextureDesc, "GBufferA"_xsh);
	const Scheduler::TextureHandle gfxSchGBufferBTexture =
		gfxSchTaskGraph.createTransientTexture(gfxHwGBufferBTextureDesc, "GBufferB"_xsh);
	const Scheduler::TextureHandle gfxSchGBufferCTexture =
		gfxSchTaskGraph.createTransientTexture(gfxHwGBufferCTextureDesc, "GBufferC"_xsh);

	const HAL::TextureDesc gfxHwLuminanceTextureDesc =
		HAL::TextureDesc::Create2DRT(targetWidth, targetHeight, HAL::TextureFormat::R16G16B16A16, 1);
	const Scheduler::TextureHandle gfxSchLuminanceTexture =
		gfxSchTaskGraph.createTransientTexture(gfxHwLuminanceTextureDesc, "Luminance"_xsh);

	const UploadBufferPointer gfxViewConstantBufferPtr = gfxSchTaskGraph.allocateTransientUploadMemory(sizeof(ViewConstantBuffer));
	{
		const float32 tanHalfFov = XLib::Math::Tan(cameraDesc.fov / 2.0f);
		const float32 aspect = float32(targetWidth) / float32(targetHeight);

		const float32 depthDeprojectB = cameraDesc.zFar / (cameraDesc.zFar - cameraDesc.zNear);
		const float32 depthDeprojectA = depthDeprojectB * cameraDesc.zNear;

		const XLib::Matrix4x4 worldToViewTransform = XLib::Matrix4x4::LookAtCentered(cameraDesc.position, cameraDesc.direction, cameraDesc.up);
		const XLib::Matrix4x4 viewToWorldTransform = XLib::Matrix4x4::Inverse(worldToViewTransform);
		const XLib::Matrix4x4 projectionMatrix = XLib::Matrix4x4::Perspective(cameraDesc.fov, aspect, cameraDesc.zNear, cameraDesc.zFar);

		ViewConstantBuffer& viewConstantBuffer = *(ViewConstantBuffer*)gfxViewConstantBufferPtr.ptr;
		viewConstantBuffer =
		{
			.worldToViewTransform = worldToViewTransform,
			.worldToClipTransform = worldToViewTransform * projectionMatrix,
			.viewToWorldTransform = viewToWorldTransform,
			.ndcDeprojectionCoefs = float32x4(tanHalfFov * aspect, tanHalfFov, depthDeprojectA, depthDeprojectB),
		};
	}

	const UploadBufferPointer gfxDeferredLightingConstantBufferPtr = gfxSchTaskGraph.allocateTransientUploadMemory(sizeof(DeferredLightingConstantBuffer));
	{
		static float a = 0.0f;
		a += 0.03f;
		const float32x3 lightDirection = XLib::VectorMath::Normalize(float32x3(XLib::Math::Sin(a), XLib::Math::Cos(a), 0.5f));

		DeferredLightingConstantBuffer& deferredLightingConstantBuffer = *(DeferredLightingConstantBuffer*)gfxDeferredLightingConstantBufferPtr.ptr;
		deferredLightingConstantBuffer =
		{
			.lightDirection = lightDirection,
		};
	}

	const UploadBufferPointer gfxTonemappingConstantBufferPtr = gfxSchTaskGraph.allocateTransientUploadMemory(sizeof(TonemappingConstantBuffer));
	{
		TonemappingConstantBuffer& tonemappingConstantBuffer = *(TonemappingConstantBuffer*)gfxTonemappingConstantBufferPtr.ptr;
		tonemappingConstantBuffer =
		{
			.exposure = 1.0f,
		};
	}

	CommonParams* commonParams = (CommonParams*)gfxSchTaskGraph.allocateUserData(sizeof(CommonParams));
	*commonParams =
	{
		.sceneRenderer = this,
		.scene = &scene,
		.targetWidth = targetWidth,
		.targetHeight = targetHeight,
		.gfxHwViewConstantBufferPtr = gfxViewConstantBufferPtr.hwPtr,
		.gfxHwDeferredLightingConstantBufferPtr = gfxDeferredLightingConstantBufferPtr.hwPtr,
		.gfxHwTonemappingConstantBufferPtr = gfxTonemappingConstantBufferPtr.hwPtr,
		.gfxSchDepthTexture = gfxSchDepthTexture,
		.gfxSchGBufferATexture = gfxSchGBufferATexture,
		.gfxSchGBufferBTexture = gfxSchGBufferBTexture,
		.gfxSchGBufferCTexture = gfxSchGBufferCTexture,
		.gfxSchLuminanceTexture = gfxSchLuminanceTexture,
		.gfxSchTargetTexture = gfxSchTargetTexture,
	};


	// Scene geometry pass.
	{
		auto SceneGeometryPassExecutor = [](Scheduler::TaskExecutionContext& gfxSchExecutionContext,
			HAL::Device& gfxHwDevice, HAL::CommandList& gfxHwCommandList, void* userData) -> void
		{
			CommonParams& params = *(CommonParams*)userData;
			params.sceneRenderer->sceneGeometryPassExecutor(gfxSchExecutionContext, gfxHwDevice, gfxHwCommandList, params);
		};

		gfxSchTaskGraph.addTask(Scheduler::TaskType::Graphics, SceneGeometryPassExecutor, commonParams)
			.addColorRenderTarget(gfxSchGBufferATexture)
			.addColorRenderTarget(gfxSchGBufferBTexture)
			.addColorRenderTarget(gfxSchGBufferCTexture)
			.addDepthStencilRenderTarget(gfxSchDepthTexture);
	}

	// Deferred ligting pass.
	{
		auto DeferredLightingPassExecutor = [](Scheduler::TaskExecutionContext& gfxSchExecutionContext,
			HAL::Device& gfxHwDevice, HAL::CommandList& gfxHwCommandList, void* userData) -> void
		{
			CommonParams& params = *(CommonParams*)userData;
			params.sceneRenderer->deferredLightingPassExecutor(gfxSchExecutionContext, gfxHwDevice, gfxHwCommandList, params);
		};

		gfxSchTaskGraph.addTask(Scheduler::TaskType::Graphics, DeferredLightingPassExecutor, commonParams)
			.addTextureShaderRead(gfxSchGBufferATexture, Scheduler::ResourceShaderAccessStage::Pixel)
			.addTextureShaderRead(gfxSchGBufferBTexture, Scheduler::ResourceShaderAccessStage::Pixel)
			.addTextureShaderRead(gfxSchGBufferCTexture, Scheduler::ResourceShaderAccessStage::Pixel)
			.addTextureShaderRead(gfxSchDepthTexture, Scheduler::ResourceShaderAccessStage::Pixel)
			.addColorRenderTarget(gfxSchLuminanceTexture);
	}

	// Tonemapping pass.
	{
		auto TonemappingPassExecutor = [](Scheduler::TaskExecutionContext& gfxSchExecutionContext,
			HAL::Device& gfxHwDevice, HAL::CommandList& gfxHwCommandList, void* userData) -> void
		{
			CommonParams& params = *(CommonParams*)userData;
			params.sceneRenderer->tonemappingPassExecutor(gfxSchExecutionContext, gfxHwDevice, gfxHwCommandList, params);
		};

		gfxSchTaskGraph.addTask(Scheduler::TaskType::Graphics, TonemappingPassExecutor, commonParams)
			.addTextureShaderRead(gfxSchLuminanceTexture, Scheduler::ResourceShaderAccessStage::Pixel)
			.addColorRenderTarget(gfxSchTargetTexture);
	}
}
