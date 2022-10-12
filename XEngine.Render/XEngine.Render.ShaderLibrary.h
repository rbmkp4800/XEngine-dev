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

		void load(const char* packPath);
		void reload(const char* packPath);

		HAL::PipelineLayoutHandle getPipelineLayout(uint64 nameXSH) const;
		HAL::PipelineHandle getPipeline(uint64 nameXSH) const;
	};
}
