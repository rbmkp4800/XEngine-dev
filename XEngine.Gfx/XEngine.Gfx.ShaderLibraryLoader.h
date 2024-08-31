#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

namespace XEngine::Gfx
{
	class ShaderLibraryLoader : public XLib::NonCopyable
	{
	private:
		struct DescriptorSetLayout;
		struct PipelineLayout;
		struct Shader;

	private:
		void* memoryBlock = nullptr;

		DescriptorSetLayout* descriptorSetLayoutTable = nullptr;
		PipelineLayout* pipelineLayoutTable = nullptr;
		Shader* shaderTable = nullptr;
		uint32 descriptorSetLayoutCount = 0;
		uint32 pipelineLayoutCount = 0;
		uint32 shaderCount = 0;

	public:
		ShaderLibraryLoader() = default;
		~ShaderLibraryLoader();

		void load(const char* libraryFilePath, HAL::Device& hwDevice);

		static HAL::DescriptorSetLayoutHandle GetDescriptorSetLayout(uint64 nameXSH) const;
		static HAL::PipelineLayoutHandle GetPipelineLayout(uint64 nameXSH) const;
		static HAL::ShaderHandle GetShader(uint64 nameXSH) const;
	};
}
