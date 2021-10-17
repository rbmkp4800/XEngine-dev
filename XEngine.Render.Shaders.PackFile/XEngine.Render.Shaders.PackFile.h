#pragma once

#include <XLib.h>

namespace XEngine::Render::Shaders::PackFile
{
	static constexpr uint32 Signature = 0;
	static constexpr uint16 CurrentVersion = 1;

	enum class Patform : uint8
	{
		D3D12,
	};

	enum class PipelineType : uint8
	{
		Graphics,
		Compute,
	};

	struct Header
	{
		uint32 signature;
		uint16 version;
		uint16 platformFlags;
		uint16 bindingLayoutCount;
		uint16 pipelineCount;
		uint32 pipelineBinaryBlobsMapSize;
		uint32 binaryBlobCount;
	};

	struct PipelineDesc
	{
		uint64 nameCRC;
		uint16 bindingLayoutIndex;
		PipelineType type;
		uint8 binaryBlobCount;
		uint32 binaryBlobsMapOffset;
	};

	struct BinaryBlobDesc
	{
		uint32 offset;
		uint32 size;
	};
}
