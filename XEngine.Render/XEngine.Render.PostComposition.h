#pragma once

#include "XEngine.Render.Scheduler.h"

namespace XEngine::Render
{
	class PostComposition abstract final
	{
	private:
		struct PassUserData;

	private:
		static void PassExecutor(Scheduler::PassExecutionBroker& broker,
			HAL::Device& device, HAL::CommandList& commandList, const void* userDataPtr);

	public:
		static void Schedule(Scheduler::Pipeline& pipeline,
			Scheduler::TextureHandle inputLuminanceTexture,
			Scheduler::TextureHandle outputColorTexture);
	};
}
