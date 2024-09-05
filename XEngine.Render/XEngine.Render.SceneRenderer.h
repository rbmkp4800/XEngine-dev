#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Vectors.h>
#include <XEngine.Gfx.HAL.D3D12.h>
#include <XEngine.Gfx.Scheduler.h>

#include "XEngine.Render.Scene.h"

namespace XEngine::Render
{
	struct CameraDesc
	{
		float32x3 position;
		float32x3 direction;
		float32x3 up;
		float32 fov;
		float32 zNear;
		float32 zFar;
	};

	class SceneRenderer : public XLib::NonCopyable
	{
	private:
		struct CommonParams;

	private:
		Gfx::HAL::DescriptorSetLayoutHandle gfxHwGBufferTexturesDSL = {};
		Gfx::HAL::DescriptorSetLayoutHandle gfxHwTonemappingInputDSL = {};

		Gfx::HAL::PipelineLayoutHandle gfxHwSceneGeometryPipelineLayout = {};
		Gfx::HAL::PipelineLayoutHandle gfxHwDeferredLightingPipelineLayout = {};
		Gfx::HAL::PipelineLayoutHandle gfxHwTonemappingPipelineLayout = {};

		Gfx::HAL::GraphicsPipelineHandle gfxHwSceneGeometryPipeline = {};
		Gfx::HAL::GraphicsPipelineHandle gfxHwDeferredLightingPipeline = {};
		Gfx::HAL::GraphicsPipelineHandle gfxHwTonemappingPipeline = {};

	private:
		void sceneGeometryPassExecutor(Gfx::Scheduler::TaskExecutionContext& gfxSchExecutionContext,
			Gfx::HAL::Device& gfxHwDevice, Gfx::HAL::CommandList& gfxHwCommandList, CommonParams& params) const;
		void deferredLightingPassExecutor(Gfx::Scheduler::TaskExecutionContext& gfxSchExecutionContext,
			Gfx::HAL::Device& gfxHwDevice, Gfx::HAL::CommandList& gfxHwCommandList, CommonParams& params) const;
		void tonemappingPassExecutor(Gfx::Scheduler::TaskExecutionContext& gfxSchExecutionContext,
			Gfx::HAL::Device& gfxHwDevice, Gfx::HAL::CommandList& gfxHwCommandList, CommonParams& params) const;

	public:
		SceneRenderer() = default;
		~SceneRenderer() = default;

		void initialize(Gfx::HAL::Device& gfxHwDevice);

		void render(const Scene& scene, const CameraDesc& cameraDesc,
			Gfx::Scheduler::TaskGraph& gfxSchTaskGraph, Gfx::Scheduler::TextureHandle gfxSchTargetTexture,
			uint16 targetWidth, uint16 targetHeight);
	};
}
