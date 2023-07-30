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
		uint16 pipelineLayoutCount;
		uint16 pipelineCount;
		uint16 bytecodeBlobCount;
		uint32 blobsDataOffset;
	};

	struct BlobRecord // 12 bytes
	{
		uint32 offset;
		uint32 size;
		uint32 checksum;
	};
	static_assert(sizeof(BlobRecord) == 12);

	struct DescriptorSetLayoutRecord // 20 bytes
	{
		uint32 nameXSH0;
		uint32 nameXSH1;
		BlobRecord blob;
	};
	static_assert(sizeof(DescriptorSetLayoutRecord) == 20);

	struct PipelineLayoutRecord // 20 bytes
	{
		uint32 nameXSH0;
		uint32 nameXSH1;
		BlobRecord blob;
	};
	static_assert(sizeof(PipelineLayoutRecord) == 20);

	struct PipelineRecord // 40 bytes
	{
		// [0..8)
		uint32 nameXSH0;
		uint32 nameXSH1;

		// [8..16)
		uint32 pipelineLayoutNameXSH0;
		uint32 pipelineLayoutNameXSH1;

		// [16..28)
		BlobRecord graphicsStateBlob;

		// [28..36)
		uint16 vsBytecodeBlobIndex;
		uint16 asBytecodeBlobIndex;
		uint16 msBytecodeBlobIndex;
		uint16 psORcsBytecodeBlobIndex;
		// Bytecode blob index should be 0xFFFF if blob is not enabled.
	};
	static_assert(sizeof(PipelineRecord) == 36);
}
