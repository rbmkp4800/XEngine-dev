#pragma once

#include <XLib.NonCopyable.h>
#include <XLib.Vectors.h>

#include <XEngine.Render.HAL.D3D12.h>

#include "XEngine.Render.Scheduler.h"

namespace XEngine::Render
{
	class SceneTransforms;
	class SceneRTAS;
	class SceneGeometry;
	class SceneAnalyticLights;
	class SceneVolumetrics;
	class SceneParticles;
	class SceneDecals;

	class SceneGeometryRenderer;
	class SceneVolumetricsRenderer;
	class SceneDecalsRenderer;

	struct GBuffer // DeferredSurfaceInfoTextures
	{
		Scheduler::TextureHandle depthTexture;
		Scheduler::TextureHandle textureA;
		Scheduler::TextureHandle textureB;
	};

	GBuffer CreateGBuffer(Scheduler::Schedule& schedule, uint16 width, uint16 height);

	struct CameraInfo
	{
		float32x3 position, forward, up;
		float32 fov, zNear, zFar;
	};

	class SceneGeometry : XLib::NonCopyable
	{
	private:

	public:
		SceneGeometry() = default;
		~SceneGeometry() = default;


	};

	class SceneGeometryRenderer abstract final
	{
	private:
		struct AccumulateDeferredLightingPassUserData;
		
	private:
		static void AccumulateDeferredLightingPassExecutor(Scheduler::PassExecutionContext& context,
			HAL::Device& device, HAL::CommandList& commandList, const void* userDataPtr);

	public:
		static void AddDrawDeferredGeometryPass(Scheduler::Schedule& schedule,
			const SceneGeometry& sceneGeometry, const CameraInfo& camera,
			const GBuffer& outputGBuffer);

		static void AddDrawForwardGeometry(Scheduler::Schedule& schedule,
			const SceneGeometry& sceneGeometry, const CameraInfo& camera,
			Scheduler::TextureHandle depthTexture,
			Scheduler::TextureHandle outputLuminanceTexture);

		static void AddAccumulateDeferredLightingPass(Scheduler::Schedule& schedule,
			//const SceneAnalyticLights& lights, const CameraInfo& camera,
			//const GBuffer& inputGBuffer,
			Scheduler::TextureHandle outputLuminanceTexture);
	};
}
