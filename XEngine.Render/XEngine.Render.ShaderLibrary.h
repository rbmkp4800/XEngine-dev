#pragma once

#include <XLib.h>

#include <XEngine.Render.HAL.D3D12.h>

namespace XEngine::Render
{
	class ShaderLibrary : public XLib::NonCopyable
	{
	private:
		struct PipelineLayout;
		struct Pipeline;

		PipelineLayout* pipelineLayoutTable = nullptr;
		Pipeline* pipelineTable = nullptr;
		uint32 pipelineLayoutCount = 0;
		uint32 pipelineCount = 0;

	public:
		ShaderLibrary() = default;
		~ShaderLibrary();

		bool load(const char* packPath);
		bool reload(const char* packPath);

		HAL::PipelineLayoutHandle getPipelineLayout(uint64 nameCRC);
		HAL::PipelineHandle getPipeline(uint64 nameCRC);
	};
}
