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
		uint16 binaryBlobCount;
	};

	struct BindingLayoutDesc
	{
		uint64 nameHash;
	};

	struct PipelineDesc
	{
		uint64 nameHash;
		uint16 bindingLayoutIndex;
		PipelineType type;
		uint8 binaryBlobCount;
	};

	struct BinaryBlobDesc
	{
		uint32 offset;
		uint32 size;
	};
}
