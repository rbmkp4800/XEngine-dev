#pragma once

#include <XLib.h>

#include <XEngine.Render.HAL.D3D12.h>

namespace XEngine::Render
{
	class ShaderLibrary : public XLib::NonCopyable
	{
	private:
		struct DescriptorSetLayout;
		struct PipelineLayout;
		struct Pipeline;

	private:
		void* memoryBlock = nullptr;

		DescriptorSetLayout* descriptorSetLayoutTable = nullptr;
		PipelineLayout* pipelineLayoutTable = nullptr;
		Pipeline* pipelineTable = nullptr;
		uint32 descriptorSetLayoutCount = 0;
		uint32 pipelineLayoutCount = 0;
		uint32 pipelineCount = 0;

	public:
		ShaderLibrary() = default;
		~ShaderLibrary();

		void load(const char* libraryFilePath);
		void reload(const char* libraryFilePath);

		HAL::DescriptorSetLayoutHandle getDescriptorSetLayout(uint64 nameXSH) const;
		HAL::PipelineLayoutHandle getPipelineLayout(uint64 nameXSH) const;
		HAL::PipelineHandle getPipeline(uint64 nameXSH) const;
	};
}
