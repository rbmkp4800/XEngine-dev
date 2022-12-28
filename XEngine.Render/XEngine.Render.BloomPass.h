#pragma once

#include "XEngine.Render.Scheduler.h"

namespace XEngine::Render
{
	class BloomPass abstract final
	{
	private:
		struct UserData
		{
			Scheduler::PipelineTextureHandle inputTexture;
			Scheduler::PipelineTextureHandle outputTexture;
		};

		static void Executor(Scheduler::PipelineSchedulerBroker& schedulerBroker,
			HAL::Device& device, HAL::CommandList& commandList, const void* userDataPtr);

	public:
		struct Output
		{
			Scheduler::PipelineTextureHandle bloomTexture;
		};

		static Output AddToPipeline(Scheduler::Pipeline& pipeline, Scheduler::PipelineTextureHandle inputTexture);
	};
}
