#pragma once

#include <XLib.NonCopyable.h>
#include <XLib.Vectors.h>

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
	public:
		static void ScheduleDrawDeferredGeometry(Scheduler::Pipeline& pipeline,
			const SceneGeometry& sceneGeometry, const CameraInfo& camera,
			const GBuffer& outputGBuffer);

		static void ScheduleDrawForwardGeometry(Scheduler::Pipeline& pipeline,
			const SceneGeometry& sceneGeometry, const CameraInfo& camera,
			Scheduler::TextureHandle depthTexture,
			Scheduler::TextureHandle outputLuminanceTexture);

		static void ScheduleAccumulateAnalyticLightsForDeferredGeometry(Scheduler::Pipeline& pipeline,
			const SceneAnalyticLights& lights, const CameraInfo& camera,
			const GBuffer& inputGBuffer,
			Scheduler::TextureHandle outputLuminanceTexture);
	};
}
