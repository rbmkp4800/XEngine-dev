#pragma once

#include <XLib.h>

//#include <XEngine.Render.HAL.D3D12.h>

namespace XEngine::Render::Device_
{
	class ShadersLoader
	{
	private:

	public:
		using PipelineToken = uint32;

	public:
		ShadersLoader() = default;
		~ShadersLoader() = default;

		PipelineToken registerPipeline(uint64 nameCRC);
		void unregisterPipeline(PipelineToken token);

		//HAL::GraphicsPipeline& acquirePipeline(PipelineToken token);
	};
}
