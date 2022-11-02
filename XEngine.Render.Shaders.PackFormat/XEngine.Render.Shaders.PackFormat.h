#pragma once

#include <XLib.h>

// TODO: Describe format properly.

namespace XEngine::Render::Shaders::PackFormat
{
	static constexpr uint32 PackSignature = 0;
	static constexpr uint16 PackCurrentVersion = 1;

	struct PackHeader
	{
		uint32 signature;
		uint16 version;

		// TODO: Size
		// TODO: Checksum

		//uint16 descriptorSetLayoutCount;
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

	struct PipelineLayoutRecord // TODO: Should be 20 bytes
	{
		uint64 nameXSH;
		//uint32 nameXSH0;
		//uint32 nameXSH1;
		BlobRecord blob;
	};
	//static_assert(sizeof(PipelineLayoutRecord) == 20);

	struct PipelineRecord // 32 bytes
	{
		// 0..7
		uint64 nameXSH;
		//uint32 nameXSH0;
		//uint32 nameXSH1;

		// 8..9
		struct
		{
			unsigned short pipelineLayoutIndex : 15;
			unsigned short isGraphics : 1;
		};

		// 10..19
		uint16 bytecodeBlobIndices[4];

		// 20..31
		BlobRecord graphicsBaseBlob;
	};
	static_assert(sizeof(PipelineRecord) == 32);
}
