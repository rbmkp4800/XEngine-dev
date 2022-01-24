#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>

#include <XEngine.Render.HAL.D3D12.h>

// NOTE: This is temporary implementation.

namespace XEngine::Render { class Device; }

namespace XEngine::Render::Device_
{
	class ShadersLoader : public XLib::NonCopyable
	{
	private:
		struct PipelineLayout
		{
			uint64 nameCRC;
			HAL::PipelineLayoutHandle halPipelineLayoutHandle;
		};

		struct Pipeline
		{
			uint64 nameCRC;
			HAL::PipelineHandle halPipelineHandle;
		};

	private:
		Device& device;

		XLib::ArrayList<PipelineLayout> pipelineLayoutsList;
		XLib::ArrayList<Pipeline> pipelinesList;

	public:
		ShadersLoader(Device& device) : device(device) {}
		~ShadersLoader() = default;

		void load(const char* packPath);

		HAL::PipelineHandle getPipeline(uint64 pipelineNameCRC) const ;
		HAL::PipelineLayoutHandle getPipelineLayout(uint64 pipelineLayoutNameCRC) const;
	};
}
