#include <XEngine.XStringHash.h>

#include "XEngine.Render.Scene.h"

#include "XEngine.Render.h"

using namespace XEngine::Render;
using namespace XEngine::Render::HAL;

struct SceneGeometryRenderer::AccumulateDeferredLightingPassUserData
{
	Scheduler::TextureHandle outputLuminanceTexture;
};

void SceneGeometryRenderer::AccumulateDeferredLightingPassExecutor(Scheduler::PassExecutionContext& context,
	Device& device, CommandList& commandList, const void* userDataPtr)
{
	const AccumulateDeferredLightingPassUserData& userData = *(const AccumulateDeferredLightingPassUserData*)userDataPtr;

	const PipelineLayoutHandle accumulateDeferredLightingPipelineLayout =
		Host::GetShaders().getPipelineLayout("AccumulateDeferredLighting"_xsh);
	const PipelineHandle accumulateDeferredLightingPipeline =
		Host::GetShaders().getPipeline("AccumulateDeferredLighting"_xsh);

	const HAL::TextureHandle halOutputLuminanceTexture = context.resolveTexture(userData.outputLuminanceTexture);

	const RenderTargetViewHandle RT = context.createTransientRenderTargetView(halOutputLuminanceTexture,
		// TODO: ...
		TexelViewFormat::R8G8B8A8_UNORM);

	struct AccumulateDeferredLightingConstants
	{
		float32x4 dummy;
	};

	const UploadMemoryAllocationInfo constantsAllocationInfo = context.allocateTransientUploadMemory(sizeof(AccumulateDeferredLightingConstants));

	static float32 qqq = 0;
	qqq += 0.1f;

	AccumulateDeferredLightingConstants& constants = *(AccumulateDeferredLightingConstants*)constantsAllocationInfo.cpuPointer;
	constants = {};
	constants.dummy = float32x4(qqq, 2, 3, 4);

	//commandList.pushDebugMarker("AccumulateDeferredLighting");

	commandList.setPipelineType(PipelineType::Graphics);
	commandList.setPipelineLayout(accumulateDeferredLightingPipelineLayout);
	commandList.setPipeline(accumulateDeferredLightingPipeline);
	commandList.setRenderTarget(RT);
	commandList.setViewport(0.0f, 0.0f, 1280.0f, 720.0f);
	commandList.setScissor(0, 0, 1280, 720);

	commandList.bindBuffer("VIEW_CONSTANTS"_xsh, BufferBindType::Constant, constantsAllocationInfo.gpuPointer);
	commandList.draw(3);

	//commandList.popDebugMarker();
}

void SceneGeometryRenderer::AddAccumulateDeferredLightingPass(Scheduler::Schedule& schedule,
	//const SceneAnalyticLights& lights, const CameraInfo& camera,
	//const GBuffer& inputGBuffer,
	Scheduler::TextureHandle outputLuminanceTexture)
{
	Scheduler::PassTextureDependencyDesc textureDependencies[] =
	{
		{ outputLuminanceTexture, Scheduler::TextureAccess::RenderTarget },
	};

	Scheduler::PassDependenciesDesc dependencies = {};
	dependencies.textureDependencies = textureDependencies;
	dependencies.textureDependencyCount = countof(textureDependencies);

	//AccumulateDeferredLightingPassUserData* userData =
	//	(AccumulateDeferredLightingPassUserData*)schedule.allocateUserData(sizeof(AccumulateDeferredLightingPassUserData));

	static AccumulateDeferredLightingPassUserData userDataStorage;
	AccumulateDeferredLightingPassUserData* userData = &userDataStorage;
	*userData = AccumulateDeferredLightingPassUserData{};
	userData->outputLuminanceTexture = outputLuminanceTexture;

	schedule.addPass(Scheduler::PassType::Graphics, dependencies, &AccumulateDeferredLightingPassExecutor, userData);
}
