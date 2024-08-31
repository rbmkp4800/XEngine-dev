#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Vectors.h>
#include <XEngine.Gfx.HAL.D3D12.h>
#include <XEngine.Gfx.Scheduler.h>

namespace XEngine::Render
{
	class SceneRenderer : public XLib::NonCopyable
	{
	private:
		Gfx::HAL::PipelineLayoutHandle gfxHwTestPipelineLayout = {};
		Gfx::HAL::GraphicsPipelineHandle gfxHwTestPipeline = {};

	private:
		struct TestPassUserData
		{
			SceneRenderer* sceneRenderer;
			Gfx::Scheduler::TextureHandle gfxSchTargetTexture;
		};

	private:
		void testPassExecutor(Gfx::Scheduler::TaskExecutionContext& gfxSchExecutionContext,
			Gfx::HAL::Device& gfxHwDevice, Gfx::HAL::CommandList& gfxHwCommandList, TestPassUserData& userData);

	public:
		SceneRenderer() = default;
		~SceneRenderer() = default;

		void initialize(Gfx::HAL::Device& gfxHwDevice);

		void render(
			Gfx::Scheduler::TaskGraph& gfxSchTaskGraph,
			Gfx::Scheduler::TextureHandle gfxSchTargetTexture);
	};
}
