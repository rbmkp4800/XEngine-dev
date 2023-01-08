#pragma once

#include "XEngine.Render.Scheduler.h"

namespace XEngine::Render
{
	class Bloom abstract final
	{
	private:
		struct PassUserData;

	private:
		static void PassExecutor(Scheduler::PassExecutionBroker& schedulerBroker,
			HAL::Device& device, HAL::CommandList& commandList, const void* userDataPtr);

	public:
		struct Output
		{
			Scheduler::TextureHandle bloom;
		};

		static Output Schedule(Scheduler::Pipeline& pipeline, Scheduler::TextureHandle inputLuminance);
	};
}
