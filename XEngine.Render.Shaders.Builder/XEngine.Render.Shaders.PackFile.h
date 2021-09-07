#pragma once

#include <XLib.h>

namespace XEngine::Render::Shaders::PackFile
{
	static constexpr uint32 Signature = 0;
	static constexpr uint16 CurrentVersion = 1;

	enum class Patform : uint8
	{
		Invalid = 0,
		PC_D3D12,
		//PC_Vulkan,
		//Scarlett,
		//Prospero,
	};

	struct Header
	{
		uint32 signature;
		uint16 version;
		uint16 platformFlags;
		uint16 rootSignatureCount;
		uint16 shaderCount;
	};

	struct RootSignatureTableEntry
	{
		uint64 nameHash;
		uint64 dataHash;
		uint32 dataOffset;
		uint32 dataSize;
	};

	struct ShaderTableEntry
	{
		uint64 nameHash;
		uint64 dataHash;
		uint32 dataOffset;
		uint32 dataSize;
		uint16 rootSignatureIndex;
		uint8 type;
		uint8 _padding0;
	};
}
