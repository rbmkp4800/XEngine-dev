#pragma once

#include <XLib.h>

namespace XEngine::Render::Shaders::PackFormat
{
	static constexpr uint32 Signature = 0;
	static constexpr uint16 CurrentVersion = 1;

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
		uint16 pipelineLayoutCount;
		uint16 pipelineCount;
		uint32 pipelineBytecodeObjectsMapSize;
		uint32 objectCount;
	};

	struct PipelineDesc
	{
		uint64 nameCRC;
		uint16 pipelineLayoutIndex;
		PipelineType type;
		uint8 bytecodeObjectCount;
		uint32 bytecodeObjectsMapOffset;
	};

	struct ObjectDesc
	{
		uint32 offset;
		uint32 size;
	};
}
