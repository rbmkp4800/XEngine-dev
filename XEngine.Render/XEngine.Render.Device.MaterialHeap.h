#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>

#include "XEngine.Render.Base.h"

// TODO: Remove redundant indirection when getting effect for material

struct ID3D12Resource;
struct ID3D12PipelineState;

namespace XEngine::Render { class Device; }

namespace XEngine::Render::Device_
{
	class MaterialHeap : public XLib::NonCopyable
	{
	private:
		struct MaterialShader
		{
			XLib::Platform::COMPtr<ID3D12PipelineState> d3dPSO;
			uint16 materialConstantsTableArenaBaseSegment;
			uint16 materialConstantsSize;
			uint16 materialUserSpecifiedConstantsOffset;
			uint16 allocatedInstanceCount;
		};

	private:
		Device& device;

		MaterialShader materialShaders[16];
		uint16 materialShaderCount = 0;
		uint16 materialCount = 0;

		XLib::Platform::COMPtr<ID3D12Resource> d3dMaterialConstantsTableArena;
		byte* mappedMaterialConstantsTableArena = nullptr;
		uint16 allocatedCommandListArenaSegmentCount = 0;

		XLib::Platform::COMPtr<ID3D12PipelineState> d3dDefaultMaterialPSO;

	public:
		MaterialHeap(Device& device) : device(device) {}
		~MaterialHeap() = default;

		void initialize();

		//MaterialShaderHandle createMaterialShader(const void* bytecode, uint32 bytecodeSize);
		MaterialShaderHandle createDefaultMaterialShader();
		void destroyMaterialShader(MaterialShaderHandle handle);

		MaterialInstanceHandle createMaterialInstance(MaterialShaderHandle shader,
			const void* initialConstants = nullptr,
			const TextureHandle* intialTextures = nullptr,
			uint8 initialTextureCount = 0);
		void releaseMaterialInstance(MaterialInstanceHandle handle);

		void updateMaterialInstanceConstants(MaterialInstanceHandle handle, uint32 offset, const void* data, uint32 size);
		void updateMaterialInstanceTexture(MaterialInstanceHandle handle, uint32 slot, TextureHandle textureHandle);

		ID3D12PipelineState* getMaterialShaderPSO(MaterialShaderHandle materialShader);
		uint64 getMaterialInstanceConstantsTableGPUAddress(MaterialShaderHandle materialShader) const;
		uint16 getMaterialInstanceConstantsTableEntryIndex(MaterialInstanceHandle materialInstance) const;

		static MaterialShaderHandle getMaterialShaderFromInstance(MaterialInstanceHandle materialInstance);

		ID3D12PipelineState* getDefaultMaterialPSO() { return d3dDefaultMaterialPSO; }
	};
}
