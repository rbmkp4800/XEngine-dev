#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Gfx.HAL.D3D12.h>

namespace XEngine::Gfx
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

		void load(const char* libraryFilePath, HAL::Device& halDevice);
		void reload(const char* libraryFilePath, HAL::Device& halDevice);

		HAL::DescriptorSetLayoutHandle getDescriptorSetLayout(uint64 nameXSH) const;
		HAL::PipelineLayoutHandle getPipelineLayout(uint64 nameXSH) const;
		HAL::PipelineHandle getPipeline(uint64 nameXSH) const;
	};
}
