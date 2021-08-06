#pragma once

#define DECLARE_D3D12PipelineStateSubobject(Name, SubobjectType, PayloadFieldType, PayloadFieldName)	\
class alignas(void*) Name {										\
private:														\
	D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type = SubobjectType;	\
public:															\
	Name() = default;											\
	PayloadFieldType PayloadFieldName;							\
};

DECLARE_D3D12PipelineStateSubobject(D3D12PipelineStateSubobjectRootSignature, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, ID3D12RootSignature*, RS);
DECLARE_D3D12PipelineStateSubobject(D3D12PipelineStateSubobjectCS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS, D3D12_SHADER_BYTECODE, bytecode);
DECLARE_D3D12PipelineStateSubobject(D3D12PipelineStateSubobjectVS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS, D3D12_SHADER_BYTECODE, bytecode);
DECLARE_D3D12PipelineStateSubobject(D3D12PipelineStateSubobjectAS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, D3D12_SHADER_BYTECODE, bytecode);
DECLARE_D3D12PipelineStateSubobject(D3D12PipelineStateSubobjectMS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, D3D12_SHADER_BYTECODE, bytecode);
DECLARE_D3D12PipelineStateSubobject(D3D12PipelineStateSubobjectPS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, D3D12_SHADER_BYTECODE, bytecode);
DECLARE_D3D12PipelineStateSubobject(D3D12PipelineStateSubobjectDepthStencil, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1, D3D12_DEPTH_STENCIL_DESC1, desc);
DECLARE_D3D12PipelineStateSubobject(D3D12PipelineStateSubobjectRenderTargetFormats, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, D3D12_RT_FORMAT_ARRAY, desc);
DECLARE_D3D12PipelineStateSubobject(D3D12PipelineStateSubobjectDepthStencilFormat, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, DXGI_FORMAT, format);

#undef DECLARE_D3D12PipelineStateSubobject
