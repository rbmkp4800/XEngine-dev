#pragma once

#include <XLib.h>

// TODO: Describe format properly.

namespace XEngine::Gfx::ShaderLibraryFormat
{
	static constexpr uint32 LibrarySignature = 0;
	static constexpr uint16 LibraryCurrentVersion = 1;

	struct LibraryHeader
	{
		uint32 signature;
		uint16 version;

		// TODO: Size
		// TODO: Checksum

		uint16 descriptorSetLayoutCount;
		uint16 pipelineBindingLayoutCount;
		uint16 shaderCount;
		uint32 blobsDataOffset;
		uint32 blobsDataSize;
	};

	struct DescriptorSetLayoutRecord // 20 bytes
	{
		uint32 nameXSH0;
		uint32 nameXSH1;
		uint32 blobOffset;
		uint32 blobSize;
		uint32 blobCRC32;
	};
	static_assert(sizeof(DescriptorSetLayoutRecord) == 20);

	struct PipelineBindingLayoutRecord // 20 bytes
	{
		uint32 nameXSH0;
		uint32 nameXSH1;
		uint32 blobOffset;
		uint32 blobSize;
		uint32 blobCRC32;
	};
	static_assert(sizeof(PipelineBindingLayoutRecord) == 20);

	struct ShaderRecord // 28 bytes
	{
		uint32 nameXSH0;
		uint32 nameXSH1;
		uint32 blobOffset;
		uint32 blobSize;
		uint32 blobCRC32;
		uint32 pipelineBindingLayoutNameXSH0;
		uint32 pipelineBindingLayoutNameXSH1;
	};
	static_assert(sizeof(ShaderRecord) == 28);
}
