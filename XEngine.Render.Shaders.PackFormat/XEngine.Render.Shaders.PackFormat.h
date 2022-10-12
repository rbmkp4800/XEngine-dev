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
		uint16 pipelineLayoutCount;
		uint16 pipelineCount;
		uint16 bytecodeObjectCount;
		uint32 objectsBaseOffset;
	};

	struct ObjectRecord // 12 bytes
	{
		uint32 offset;
		uint32 size;
		uint32 crc32; // CRC-32/zlib
	};

	struct PipelineLayoutRecord // 20 bytes
	{
		uint64 nameXSH;
		ObjectRecord object;
	};

	struct PipelineRecord // 32 bytes
	{
		// 0..7
		uint64 nameXSH;

		// 8..9
		struct
		{
			uint pipelineLayoutIndex : 15;
			bool isGraphics : 1;
		};

		// 10..19
		uint16 bytecodeObjectsIndices[5];

		// 20..31
		ObjectRecord graphicsBaseObject;
	};
}
