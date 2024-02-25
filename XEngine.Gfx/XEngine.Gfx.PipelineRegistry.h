#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

namespace XEngine::Gfx
{
	class PipelineRegistry : public XLib::NonCopyable
	{
	public:
		struct GraphicsPipelineDesc
		{
			uint64 vsNameXSH;
			uint64 asNameXSH;
			uint64 msNameXSH;
			uint64 psNameXSH;
		};

	private:

	public:
		void registerStaticPipeline();
		void registerDynamicPipeline();
		void unregisterDynamicPipeline();

		//void unloadPipelinesFlagedAsOutdated();
		//void loadMissingPipelines();
	};
}
