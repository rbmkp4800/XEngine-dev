#pragma once

inline D3D12_COMMAND_QUEUE_DESC D3D12CommandQueueDesc(
	D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT,
	D3D12_COMMAND_QUEUE_FLAGS flags = D3D12_COMMAND_QUEUE_FLAG_NONE)
{
	D3D12_COMMAND_QUEUE_DESC desc;
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = flags;
	desc.NodeMask = 0;
	return desc;
}

inline D3D12_DESCRIPTOR_HEAP_DESC D3D12DescriptorHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE type,
	UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.Type = type;
	desc.NumDescriptors = numDescriptors;
	desc.Flags = flags;
	desc.NodeMask = 0;
	return desc;
}

inline D3D12_CPU_DESCRIPTOR_HANDLE operator + (D3D12_CPU_DESCRIPTOR_HANDLE handle, SIZE_T value)
{
	D3D12_CPU_DESCRIPTOR_HANDLE result;
	result.ptr = handle.ptr + value;
	return result;
}
inline D3D12_GPU_DESCRIPTOR_HANDLE operator + (D3D12_GPU_DESCRIPTOR_HANDLE handle, SIZE_T value)
{
	D3D12_GPU_DESCRIPTOR_HANDLE result;
	result.ptr = handle.ptr + value;
	return result;
}

inline D3D12_ROOT_SIGNATURE_DESC D3D12RootSignatureDesc(
	UINT numParameters = 0, const D3D12_ROOT_PARAMETER* parameters = nullptr,
	UINT numStaticSamplers = 0, const D3D12_STATIC_SAMPLER_DESC* staticSamplers = nullptr,
	D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE)
{
	D3D12_ROOT_SIGNATURE_DESC desc;
	desc.NumParameters = numParameters;
	desc.pParameters = parameters;
	desc.NumStaticSamplers = numStaticSamplers;
	desc.pStaticSamplers = staticSamplers;
	desc.Flags = flags;
	return desc;
}

inline D3D12_HEAP_PROPERTIES D3D12HeapProperties(D3D12_HEAP_TYPE type)
{
	D3D12_HEAP_PROPERTIES props;
	props.Type = type;
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	props.CreationNodeMask = 0;
	props.VisibleNodeMask = 0;
	return props;
}

inline D3D12_RESOURCE_DESC D3D12ResourceDesc_Buffer(UINT64 size,
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
	UINT64 alignment = 0)
{
	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = alignment;
	desc.Width = size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = flags;
	return desc;
}

inline D3D12_RESOURCE_DESC D3D12ResourceDesc_Texture2D(DXGI_FORMAT format,
	UINT width, UINT height, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
	UINT16 mipLevels = 0, UINT16 arraySize = 1)
{
	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Alignment = 0;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = arraySize;
	desc.MipLevels = mipLevels;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = flags;
	return desc;
}

inline D3D12_RESOURCE_DESC D3D12ResourceDesc_Texture3D(DXGI_FORMAT format,
	UINT width, UINT height, UINT depth,
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, UINT mipLevels = 0)
{
	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	desc.Alignment = 0;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = depth;
	desc.MipLevels = mipLevels;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = flags;
	return desc;
}

inline D3D12_RANGE D3D12Range(SIZE_T begin = 0, SIZE_T end = 0)
{
	D3D12_RANGE range;
	range.Begin = begin;
	range.End = end;
	return range;
}

inline D3D12_VIEWPORT D3D12ViewPort(float topLeftX, float topLeftY,
	float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f)
{
	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = topLeftX;
	viewport.TopLeftY = topLeftY;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = minDepth;
	viewport.MaxDepth = maxDepth;
	return viewport;
}

inline D3D12_RECT D3D12Rect(LONG left, LONG top, LONG right, LONG bottom)
{
	D3D12_RECT rect;
	rect.left = left;
	rect.top = top;
	rect.right = right;
	rect.bottom = bottom;
	return rect;
}

inline D3D12_INPUT_LAYOUT_DESC D3D12InputLayoutDesc(
	const D3D12_INPUT_ELEMENT_DESC *inputElemetDescs = nullptr, UINT numElements = 0)
{
	D3D12_INPUT_LAYOUT_DESC desc;
	desc.pInputElementDescs = inputElemetDescs;
	desc.NumElements = numElements;
	return desc;
}

inline D3D12_SHADER_BYTECODE D3D12ShaderBytecode(const void *data, SIZE_T length)
{
	D3D12_SHADER_BYTECODE bytecode;
	bytecode.pShaderBytecode = data;
	bytecode.BytecodeLength = length;
	return bytecode;
}

inline D3D12_BLEND_DESC D3D12BlendDesc_NoBlend()
{
	D3D12_BLEND_DESC desc;
	desc.AlphaToCoverageEnable = false;
	desc.IndependentBlendEnable = false;
	D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		false, false,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		desc.RenderTarget[i] = defaultRenderTargetBlendDesc;
	return desc;
}

inline D3D12_BLEND_DESC D3D12BlendDesc_DefaultBlend()
{
	D3D12_BLEND_DESC desc = {};
	desc.AlphaToCoverageEnable = false;
	desc.IndependentBlendEnable = false;
	D3D12_RENDER_TARGET_BLEND_DESC blendDesc =
	{
		true, false,
		D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ZERO, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	desc.RenderTarget[0] = blendDesc;
	return desc;
}

inline D3D12_RASTERIZER_DESC D3D12RasterizerDesc_Default()
{
	D3D12_RASTERIZER_DESC desc;
	desc.FillMode = D3D12_FILL_MODE_SOLID;
	desc.CullMode = D3D12_CULL_MODE_BACK;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	desc.DepthClipEnable = TRUE;
	desc.MultisampleEnable = FALSE;
	desc.AntialiasedLineEnable = FALSE;
	desc.ForcedSampleCount = 0;
	desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	return desc;
}

inline D3D12_RASTERIZER_DESC D3D12RasterizerDesc(D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK,
	D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID, bool depthClipEnable = true)
{
	D3D12_RASTERIZER_DESC desc;
	desc.FillMode = fillMode;
	desc.CullMode = cullMode;
	desc.FrontCounterClockwise = FALSE;
	desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	desc.DepthClipEnable = depthClipEnable;
	desc.MultisampleEnable = FALSE;
	desc.AntialiasedLineEnable = FALSE;
	desc.ForcedSampleCount = 0;
	desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	return desc;
}

inline D3D12_DEPTH_STENCIL_DESC D3D12DepthStencilDesc_Default()
{
	D3D12_DEPTH_STENCIL_DESC desc = {};
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	desc.StencilEnable = FALSE;
	desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	return desc;
}

inline D3D12_DEPTH_STENCIL_DESC D3D12DepthStencilDesc_Disable()
{
	D3D12_DEPTH_STENCIL_DESC desc = {};
	desc.DepthEnable = FALSE;
	desc.StencilEnable = FALSE;
	return desc;
}

inline D3D12_DEPTH_STENCIL_DESC D3D12DepthStencilDesc_DisableWrite()
{
	D3D12_DEPTH_STENCIL_DESC desc = {};
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	desc.StencilEnable = FALSE;
	desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	return desc;
}

inline D3D12_DEPTH_STENCIL_DESC D3D12DepthStencilDesc_DisableRead()
{
	D3D12_DEPTH_STENCIL_DESC desc = {};
	desc.DepthEnable = TRUE;
	desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	desc.StencilEnable = FALSE;
	desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	return desc;
}

inline D3D12_RESOURCE_BARRIER D3D12ResourceBarrier_Transition(ID3D12Resource *d3dResource,
	D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter,
	UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
	D3D12_RESOURCE_BARRIER_FLAGS barrierFlags = D3D12_RESOURCE_BARRIER_FLAG_NONE)
{
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = barrierFlags;
	barrier.Transition.pResource = d3dResource;
	barrier.Transition.Subresource = subresource;
	barrier.Transition.StateBefore = stateBefore;
	barrier.Transition.StateAfter = stateAfter;
	return barrier;
}

inline D3D12_VERTEX_BUFFER_VIEW D3D12VertexBufferView(
	D3D12_GPU_VIRTUAL_ADDRESS address, UINT size, UINT stride)
{
	D3D12_VERTEX_BUFFER_VIEW view;
	view.BufferLocation = address;
	view.SizeInBytes = size;
	view.StrideInBytes = stride;
	return view;
}
inline D3D12_INDEX_BUFFER_VIEW D3D12IndexBufferView(
	D3D12_GPU_VIRTUAL_ADDRESS address, UINT size, DXGI_FORMAT format)
{
	D3D12_INDEX_BUFFER_VIEW view;
	view.BufferLocation = address;
	view.SizeInBytes = size;
	view.Format = format;
	return view;
}

inline D3D12_ROOT_PARAMETER D3D12RootParameter_CBV(UINT shaderRegister,
	UINT registerSpace, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
{
	D3D12_ROOT_PARAMETER param;
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace = registerSpace;
	param.ShaderVisibility = visibility;
	return param;
}

inline D3D12_ROOT_PARAMETER D3D12RootParameter_SRV(UINT shaderRegister,
	UINT registerSpace, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
{
	D3D12_ROOT_PARAMETER param;
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace = registerSpace;
	param.ShaderVisibility = visibility;
	return param;
}

inline D3D12_ROOT_PARAMETER D3D12RootParameter_UAV(UINT shaderRegister,
	UINT registerSpace, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
{
	D3D12_ROOT_PARAMETER param;
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
	param.Descriptor.ShaderRegister = shaderRegister;
	param.Descriptor.RegisterSpace = registerSpace;
	param.ShaderVisibility = visibility;
	return param;
}

inline D3D12_ROOT_PARAMETER D3D12RootParameter_Table(UINT numDescriptorRanges,
	const D3D12_DESCRIPTOR_RANGE* descriptorRanges, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
{
	D3D12_ROOT_PARAMETER param;
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	param.DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
	param.DescriptorTable.pDescriptorRanges = descriptorRanges;
	param.ShaderVisibility = visibility;
	return param;
}

inline D3D12_ROOT_PARAMETER D3D12RootParameter_Constants(UINT num32bitValues,
	UINT shaderRegister, UINT registerSpace, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
{
	D3D12_ROOT_PARAMETER param;
	param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	param.Constants.ShaderRegister = shaderRegister;
	param.Constants.RegisterSpace = registerSpace;
	param.Constants.Num32BitValues = num32bitValues;
	param.ShaderVisibility = visibility;
	return param;
}

inline D3D12_CLEAR_VALUE D3D12ClearValue_DepthStencil(DXGI_FORMAT format,
	FLOAT depth, UINT8 stencil = 0)
{
	D3D12_CLEAR_VALUE value;
	value.Format = format;
	value.DepthStencil.Depth = depth;
	value.DepthStencil.Stencil = stencil;
	return value;
}

inline D3D12_CLEAR_VALUE D3D12ClearValue_Color(DXGI_FORMAT format,
	FLOAT r = 0.0f, FLOAT g = 0.0f, FLOAT b = 0.0f, FLOAT a = 0.0f)
{
	D3D12_CLEAR_VALUE value;
	value.Format = format;
	value.Color[0] = r;
	value.Color[1] = g;
	value.Color[2] = b;
	value.Color[3] = a;
	return value;
}

inline D3D12_SHADER_RESOURCE_VIEW_DESC D3D12ShaderResourceViewDesc_Texture2D(
	DXGI_FORMAT format, UINT mostDetailedMip = 0, UINT mipLevels = -1)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Texture2D.MostDetailedMip = mostDetailedMip;
	desc.Texture2D.MipLevels = mipLevels;
	return desc;
}

inline D3D12_SHADER_RESOURCE_VIEW_DESC D3D12ShaderResourceViewDesc_Texture2DArray(
	DXGI_FORMAT format, UINT firstArraySlice, UINT arraySize, UINT planeSlice = 0,
	UINT mostDetailedMip = 0, UINT mipLevels = -1)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.Texture2DArray.MostDetailedMip = mostDetailedMip;
	desc.Texture2DArray.MipLevels = mipLevels;
	desc.Texture2DArray.FirstArraySlice = firstArraySlice;
	desc.Texture2DArray.ArraySize = arraySize;
	desc.Texture2DArray.PlaneSlice = planeSlice;
	desc.Texture2DArray.ResourceMinLODClamp = 0.0f;
	return desc;
}

inline D3D12_SHADER_RESOURCE_VIEW_DESC D3D12ShaderResourceViewDesc_TextureCube(
	DXGI_FORMAT format, UINT mostDetailedMip = 0, UINT mipLevels = -1)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	desc.TextureCube.MostDetailedMip = mostDetailedMip;
	desc.TextureCube.MipLevels = mipLevels;
	desc.TextureCube.ResourceMinLODClamp = 0.0f;
	return desc;
}

inline D3D12_RENDER_TARGET_VIEW_DESC D3D12RenderTargetViewDesc_Texture2D(
	UINT mipSlice, UINT planeSlice = 0, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN)
{
	D3D12_RENDER_TARGET_VIEW_DESC desc = {};
	desc.Format = format;
	desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipSlice = mipSlice;
	desc.Texture2D.PlaneSlice = planeSlice;
	return desc;
}

inline D3D12_DEPTH_STENCIL_VIEW_DESC D3D12DepthStencilViewDesc_Texture2D(
	DXGI_FORMAT format, D3D12_DSV_FLAGS flags = D3D12_DSV_FLAG_NONE, UINT mipSlice = 0)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC desc;
	desc.Format = format;
	desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	desc.Flags = D3D12_DSV_FLAG_NONE;
	desc.Texture2D.MipSlice = mipSlice;
	return desc;
}

inline D3D12_DEPTH_STENCIL_VIEW_DESC D3D12DepthStencilViewDesc_Texture2DArray(
	DXGI_FORMAT format, UINT mipSlice = 0, UINT firstArraySlice = 0, UINT arraySize = 1,
	D3D12_DSV_FLAGS flags = D3D12_DSV_FLAG_NONE)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC desc;
	desc.Format = format;
	desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
	desc.Flags = D3D12_DSV_FLAG_NONE;
	desc.Texture2DArray.MipSlice = mipSlice;
	desc.Texture2DArray.FirstArraySlice = firstArraySlice;
	desc.Texture2DArray.ArraySize = arraySize;
	return desc;
}

inline D3D12_SAMPLER_DESC D3D12SamplerDesc_Default()
{
	D3D12_SAMPLER_DESC desc = {};
	desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.MipLODBias = 0.0f;
	desc.MaxAnisotropy = 1;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	desc.MinLOD = -3.402823466e+38f;
	desc.MaxLOD = 3.402823466e+38f;
	return desc;
}

inline D3D12_STATIC_SAMPLER_DESC D3D12StaticSamplerDesc_Default(UINT shaderRegister,
	UINT registerSpace, D3D12_SHADER_VISIBILITY visibility)
{
	D3D12_STATIC_SAMPLER_DESC desc;
	desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.MipLODBias = 0.0f;
	desc.MaxAnisotropy = 1;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	desc.MinLOD = -3.402823466e+38f;
	desc.MaxLOD = 3.402823466e+38f;
	desc.ShaderRegister = shaderRegister;
	desc.RegisterSpace = registerSpace;
	desc.ShaderVisibility = visibility;
	return desc;
}

inline D3D12_STATIC_SAMPLER_DESC D3D12StaticSamplerDesc_DisableMIPs(UINT shaderRegister,
	UINT registerSpace, D3D12_SHADER_VISIBILITY visibility)
{
	D3D12_STATIC_SAMPLER_DESC desc;
	desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	desc.MipLODBias = 0.0f;
	desc.MaxAnisotropy = 1;
	desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	desc.MinLOD = 0.0f;
	desc.MaxLOD = 0.0f;
	desc.ShaderRegister = shaderRegister;
	desc.RegisterSpace = registerSpace;
	desc.ShaderVisibility = visibility;
	return desc;
}

inline D3D12_STATIC_SAMPLER_DESC D3D12StaticSamplerDesc(UINT shaderRegister,
	UINT registerSpace, D3D12_SHADER_VISIBILITY visibility, D3D12_FILTER filter,
	D3D12_TEXTURE_ADDRESS_MODE addressMode,
	D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_NEVER)
{
	D3D12_STATIC_SAMPLER_DESC desc;
	desc.Filter = filter;
	desc.AddressU = addressMode;
	desc.AddressV = addressMode;
	desc.AddressW = addressMode;
	desc.MipLODBias = 0.0f;
	desc.MaxAnisotropy = 1;
	desc.ComparisonFunc = comparisonFunc;
	desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	desc.MinLOD = 0.0f;
	desc.MaxLOD = 0.0f;
	desc.ShaderRegister = shaderRegister;
	desc.RegisterSpace = registerSpace;
	desc.ShaderVisibility = visibility;
	return desc;
}

inline D3D12_COMMAND_SIGNATURE_DESC D3D12CommandSignatureDesc(UINT stride,
	UINT numArgumentDescs, const D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs)
{
	D3D12_COMMAND_SIGNATURE_DESC desc;
	desc.ByteStride = stride;
	desc.NumArgumentDescs = numArgumentDescs;
	desc.pArgumentDescs = argumentDescs;
	desc.NodeMask = 0;
	return desc;
}

inline D3D12_INDIRECT_ARGUMENT_DESC D3D12IndirectArgumentDesc_VBV(UINT slot)
{
	D3D12_INDIRECT_ARGUMENT_DESC desc;
	desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
	desc.VertexBuffer.Slot = slot;
	return desc;
}

inline D3D12_INDIRECT_ARGUMENT_DESC D3D12IndirectArgumentDesc_IBV()
{
	D3D12_INDIRECT_ARGUMENT_DESC desc;
	desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
	return desc;
}

inline D3D12_INDIRECT_ARGUMENT_DESC D3D12IndirectArgumentDesc_Constants(UINT rootParameterIndex,
	UINT destOffsetIn32BitValues, UINT num32BitValuesToSet)
{
	D3D12_INDIRECT_ARGUMENT_DESC desc;
	desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
	desc.Constant.RootParameterIndex = rootParameterIndex;
	desc.Constant.DestOffsetIn32BitValues = destOffsetIn32BitValues;
	desc.Constant.Num32BitValuesToSet = num32BitValuesToSet;
	return desc;
}

inline D3D12_INDIRECT_ARGUMENT_DESC D3D12IndirectArgumentDesc_DrawIndexed()
{
	D3D12_INDIRECT_ARGUMENT_DESC desc;
	desc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	return desc;
}

inline D3D12_UNORDERED_ACCESS_VIEW_DESC D3D12UnorderedAccessViewDesc_Buffer(
	UINT64 firstElement, UINT numElements, UINT stride, UINT64 counterOffset = 0)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = firstElement;
	desc.Buffer.NumElements = numElements;
	desc.Buffer.StructureByteStride = stride;
	desc.Buffer.CounterOffsetInBytes = counterOffset;
	desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	return desc;
}

inline D3D12_COMPUTE_PIPELINE_STATE_DESC D3D12ComputePipelineStateDesc(ID3D12RootSignature *d3dRS, const D3D12_SHADER_BYTECODE& cs)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = d3dRS;
	desc.CS = cs;
	return desc;
}

inline D3D12_BOX D3D12Box(UINT left, UINT right, UINT top = 0, UINT bottom = 1, UINT front = 0, UINT back = 1)
{
	D3D12_BOX box;
	box.left = left;
	box.top = top;
	box.front = front;
	box.right = right;
	box.bottom = bottom;
	box.back = back;
	return box;
}

inline D3D12_TEXTURE_COPY_LOCATION D3D12TextureCopyLocation_Subresource(
	ID3D12Resource *d3dResource, UINT subresourceIndex = 0)
{
	D3D12_TEXTURE_COPY_LOCATION location = {};
	location.pResource = d3dResource;
	location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	location.SubresourceIndex = subresourceIndex;
	return location;
}

inline D3D12_TEXTURE_COPY_LOCATION D3D12TextureCopyLocation_PlacedFootprint(
	ID3D12Resource *d3dResource, UINT64 offset, DXGI_FORMAT format, UINT width,
	UINT height, UINT depth, UINT rowPitch)
{
	D3D12_TEXTURE_COPY_LOCATION location = {};
	location.pResource = d3dResource;
	location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	location.PlacedFootprint.Offset = offset;
	location.PlacedFootprint.Footprint.Format = format;
	location.PlacedFootprint.Footprint.Width = width;
	location.PlacedFootprint.Footprint.Height = height;
	location.PlacedFootprint.Footprint.Depth = depth;
	location.PlacedFootprint.Footprint.RowPitch = rowPitch;
	return location;
}

inline D3D12_QUERY_HEAP_DESC D3D12QueryHeapDesc(D3D12_QUERY_HEAP_TYPE type, UINT count)
{
	D3D12_QUERY_HEAP_DESC desc = {};
	desc.Type = type;
	desc.Count = count;
	desc.NodeMask = 0;
	return desc;
}