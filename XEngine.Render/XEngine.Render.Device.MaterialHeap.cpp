#include <d3d12.h>

#include "D3D12Helpers.h"

#include <XLib.Platform.D3D12.Helpers.h>

#include "XEngine.Render.Device.MaterialHeap.h"

#include "XEngine.Render.Device.h"
#include "XEngine.Render.Internal.Shaders.h"

using namespace XLib;
using namespace XLib::Platform;
using namespace XEngine::Render;
using namespace XEngine::Render::Device_;
using namespace XEngine::Render::Internal;

static constexpr uint32 materialTableArenaSegmentSize = 0x1000;

namespace
{
	struct MaterialShaderPSOStream
	{
		D3D12PipelineStateSubobjectRootSignature subobjRS;
		D3D12PipelineStateSubobjectAS subobjAS;
		D3D12PipelineStateSubobjectMS subobjMS;
		D3D12PipelineStateSubobjectPS subobjPS;
		D3D12PipelineStateSubobjectRenderTargetFormats subobjRTFormats;
		D3D12PipelineStateSubobjectDepthStencilFormat subobjDSFormat;
		D3D12PipelineStateSubobjectDepthStencil subobjDS;
	};
}

void MaterialHeap::initialize()
{
	device.d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Buffer(0x10000),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		d3dMaterialConstantsTableArena.uuid(), d3dMaterialConstantsTableArena.voidInitRef());

	d3dMaterialConstantsTableArena->Map(0, &D3D12Range(), to<void**>(&mappedMaterialConstantsTableArena));
}

inline D3D12_DEPTH_STENCIL_DESC1 D3D12DepthStencilDesc1_Default()
{
	D3D12_DEPTH_STENCIL_DESC1 desc = {};
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	desc.StencilEnable = FALSE;
	desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	return desc;
}

MaterialShaderHandle MaterialHeap::createDefaultMaterialShader()
{
	const ShaderData& as = Shaders::SceneGeometryAS;
	const ShaderData& ms = Shaders::DefaultMaterialMS;
	const ShaderData& ps = Shaders::DefaultMaterialPS;

	MaterialShaderPSOStream psoStream = {};
	psoStream.subobjRS.RS = device.renderer.getGBufferPassD3DRS();
	psoStream.subobjAS.bytecode = { as.data, as.size };
	psoStream.subobjMS.bytecode = { ms.data, ms.size };
	psoStream.subobjPS.bytecode = { ps.data, ps.size };
	psoStream.subobjRTFormats.desc.NumRenderTargets = 2;
	psoStream.subobjRTFormats.desc.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoStream.subobjRTFormats.desc.RTFormats[1] = DXGI_FORMAT_R16G16B16A16_SNORM;
	psoStream.subobjDSFormat.format = DXGI_FORMAT_D32_FLOAT;
	psoStream.subobjDS.desc = D3D12DepthStencilDesc1_Default();

	D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
	streamDesc.SizeInBytes = sizeof(psoStream);
	streamDesc.pPipelineStateSubobjectStream = &psoStream;

	device.d3dDevice->CreatePipelineState(&streamDesc, d3dDefaultMaterialPSO.uuid(), d3dDefaultMaterialPSO.voidInitRef());

	return 0;
}

#if 0

{
	/*D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = device.sceneRenderer.getGBufferPassD3DRS();
	psoDesc.VS = D3D12ShaderBytecode(Shaders::Effect_NormalVS.data, Shaders::Effect_NormalVS.size);
	psoDesc.PS = D3D12ShaderBytecode(Shaders::Effect_PerMaterialAlbedoRoughtnessMetalnessPS.data, Shaders::Effect_PerMaterialAlbedoRoughtnessMetalnessPS.size);
	psoDesc.BlendState = D3D12BlendDesc_NoBlend();
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.RasterizerState = D3D12RasterizerDesc_Default();
	psoDesc.DepthStencilState = D3D12DepthStencilDesc_Default();
	psoDesc.InputLayout = D3D12InputLayoutDesc(inputElementDescs, countof(inputElementDescs));
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 2;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_SNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;*/

	COMPtr<ID3D12PipelineState> d3dPSO;
	device.d3dDevice->CreateGraphicsPipelineState(&psoDesc, d3dPSO.uuid(), d3dPSO.voidInitRef());

	EffectHandle result = EffectHandle(effectCount);
	effectCount++;

	Effect &effect = effects[result];
	effect.d3dPSO = move(d3dPSO);
	effect.materialConstantsTableArenaBaseSegment = allocatedCommandListArenaSegmentCount;
	effect.materialConstantsSize = 32;
	effect.materialUserSpecifiedConstantsOffset = 0;
	effect.constantsUsed = 0;
	allocatedCommandListArenaSegmentCount++;

	return result;
}

MaterialHandle MaterialHeap::createMaterial(EffectHandle effectHandle,
	const void* initialConstants, const TextureHandle* intialTextures)
{
	MaterialHandle result = MaterialHandle(materialCount);
	materialCount++;

	Effect &effect = effects[effectHandle];
	uint16 constantsIndex = effect.constantsUsed;
	effect.constantsUsed++;

	materials[result].effectHandle = effectHandle;
	materials[result].constantsIndex = constantsIndex;

	return result;
}

void MaterialHeap::updateMaterialConstants(MaterialHandle handle,
	uint32 offset, const void* data, uint32 size)
{
	const Material &material = materials[handle];
	const Effect &effect = effects[material.effectHandle];

	uint32 materialConstantsAbsoluteOffset =
		effect.materialConstantsTableArenaBaseSegment * materialTableArenaSegmentSize +
		effect.materialConstantsSize * material.constantsIndex;

	byte *materialConstants = mappedMaterialConstantsTableArena + materialConstantsAbsoluteOffset;
	Memory::Copy(materialConstants + effect.materialUserSpecifiedConstantsOffset + offset, data, size);
}

void MaterialHeap::updateMaterialTexture(MaterialHandle handle, uint32 slot, TextureHandle textureHandle)
{
	const Material &material = materials[handle];
	const Effect &effect = effects[material.effectHandle];

	uint32 materialConstantsAbsoluteOffset =
		effect.materialConstantsTableArenaBaseSegment * materialTableArenaSegmentSize +
		effect.materialConstantsSize * material.constantsIndex;

	uint32 *slots = to<uint32*>(mappedMaterialConstantsTableArena + materialConstantsAbsoluteOffset);
	slots[slot] = device.textureHeap.getSRVDescriptorIndex(textureHandle);
}

uint16 MaterialHeap::getMaterialConstantsTableEntryIndex(MaterialHandle handle) const
{
	return materials[handle].constantsIndex;
}

EffectHandle MaterialHeap::getEffect(MaterialHandle handle) const
{
	return materials[handle].effectHandle;
}

ID3D12PipelineState* MaterialHeap::getEffectPSO(EffectHandle handle)
{
	return effects[handle].d3dPSO;
}

uint64 MaterialHeap::getMaterialConstantsTableGPUAddress(EffectHandle handle)
{
	return d3dMaterialConstantsTableArena->GetGPUVirtualAddress() +
		effects[handle].materialConstantsTableArenaBaseSegment * materialTableArenaSegmentSize;
}

#endif
