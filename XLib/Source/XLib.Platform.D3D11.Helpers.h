#pragma once

inline D3D11_BUFFER_DESC D3D11BufferDesc(UINT size, UINT bindFlags, UINT structureByteStride = 0,
	UINT miscFlags = 0, D3D11_USAGE usage = D3D11_USAGE_DEFAULT, UINT cpuAccessFlags = 0)
{
	D3D11_BUFFER_DESC result;
	result.ByteWidth = size;
	result.Usage = usage;
	result.BindFlags = bindFlags;
	result.CPUAccessFlags = cpuAccessFlags;
	result.MiscFlags = miscFlags;
	result.StructureByteStride = structureByteStride;
	return result;
}

inline D3D11_TEXTURE2D_DESC D3D11Texture2DDesc(UINT width, UINT height, DXGI_FORMAT format,
	UINT bindFlags = D3D11_BIND_SHADER_RESOURCE)
{
	D3D11_TEXTURE2D_DESC result;
	result.Width = width;
	result.Height = height;
	result.MipLevels = 0;
	result.ArraySize = 1;
	result.Format = format;
	result.SampleDesc.Count = 1;
	result.SampleDesc.Quality = 0;
	result.Usage = D3D11_USAGE_DEFAULT;
	result.BindFlags = bindFlags;
	result.CPUAccessFlags = 0;
	result.MiscFlags = 0;
	return result;
}

inline D3D11_BOX D3D11Box(UINT left, UINT right, UINT top = 0, UINT bottom = 1, UINT front = 0, UINT back = 1)
{
	D3D11_BOX result;
	result.left = left;
	result.top = top;
	result.front = front;
	result.right = right;
	result.bottom = bottom;
	result.back = back;
	return result;
}

inline D3D11_RASTERIZER_DESC D3D11RasterizerDesc(D3D11_FILL_MODE fillMode = D3D11_FILL_SOLID, D3D11_CULL_MODE cullMode = D3D11_CULL_BACK,
	bool frontCounterClockwise = false, UINT depthBias = 0, float depthBiasClamp = 0.0f, float slopeScaledDepthBias = 0.0f,
	bool depthClipEnable = true, bool scissorEnable = false, bool multisampleEnable = false, bool antialiasedLineEnable = false)
{
	D3D11_RASTERIZER_DESC desc;
	desc.FillMode = fillMode;
	desc.CullMode = cullMode;
	desc.FrontCounterClockwise = frontCounterClockwise;
	desc.DepthBias = depthBias;
	desc.DepthBiasClamp = depthBiasClamp;
	desc.SlopeScaledDepthBias = slopeScaledDepthBias;
	desc.DepthClipEnable = depthClipEnable;
	desc.ScissorEnable = scissorEnable;
	desc.MultisampleEnable = multisampleEnable;
	desc.AntialiasedLineEnable = antialiasedLineEnable;
	return desc;
}

inline D3D11_BLEND_DESC D3D11BlendDesc(D3D11_BLEND scrBlend, D3D11_BLEND_OP blendOp, D3D11_BLEND dstBlend,
	D3D11_BLEND scrBlendA, D3D11_BLEND_OP blendOpA, D3D11_BLEND dstBlendA)
{
	D3D11_BLEND_DESC desc = { 0 };
	desc.RenderTarget[0].BlendEnable = true;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	//finalColor.rgb = (srcColor.rgb * SrcBlend) (BlendOp) (destColor.rgb * DestBlend);
	//finalColor.a = (srcColor.a * SrcBlendAlpha) (BlendOpAlpha) (destColor.a * DestBlendAlpha);

	desc.RenderTarget[0].SrcBlend = scrBlend;
	desc.RenderTarget[0].BlendOp = blendOp;
	desc.RenderTarget[0].DestBlend = dstBlend;

	desc.RenderTarget[0].SrcBlendAlpha = scrBlendA;
	desc.RenderTarget[0].BlendOpAlpha = blendOpA;
	desc.RenderTarget[0].DestBlendAlpha = dstBlendA;

	return desc;
}

inline D3D11_SAMPLER_DESC D3D11SamplerDesc(D3D11_FILTER filter,
	D3D11_TEXTURE_ADDRESS_MODE addressU = D3D11_TEXTURE_ADDRESS_CLAMP,
	D3D11_TEXTURE_ADDRESS_MODE addressV = D3D11_TEXTURE_ADDRESS_CLAMP,
	D3D11_TEXTURE_ADDRESS_MODE addressW = D3D11_TEXTURE_ADDRESS_CLAMP,
	UINT maxAnisotropy = 0, float *borderColor = nullptr,
	float mipLODBias = 0.0f, D3D11_COMPARISON_FUNC comparisonFunc = D3D11_COMPARISON_ALWAYS,
	float minLOD = 0.0f, float maxLOD = 3.402823466e+38f)
{
	D3D11_SAMPLER_DESC desc;
	desc.Filter = filter;
	desc.AddressU = addressU;
	desc.AddressV = addressV;
	desc.AddressW = addressW;
	desc.MipLODBias = mipLODBias;
	desc.MaxAnisotropy = maxAnisotropy;
	desc.ComparisonFunc = comparisonFunc;
	if (borderColor)
	{
		desc.BorderColor[0] = borderColor[0];
		desc.BorderColor[1] = borderColor[1];
		desc.BorderColor[2] = borderColor[2];
		desc.BorderColor[3] = borderColor[3];
	}
	else
	{
		desc.BorderColor[0] = 0.0f;
		desc.BorderColor[1] = 0.0f;
		desc.BorderColor[2] = 0.0f;
		desc.BorderColor[3] = 0.0f;
	}
	desc.MinLOD = minLOD;
	desc.MaxLOD = maxLOD;
	return desc;
}

inline D3D11_VIEWPORT D3D11ViewPort(float topLeftX, float topLeftY, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f)
{
	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = topLeftX;
	viewport.TopLeftY = topLeftY;
	viewport.Width = width;
	viewport.Height = height;
	viewport.MinDepth = minDepth;
	viewport.MaxDepth = maxDepth;
	return viewport;
}

inline D3D11_SUBRESOURCE_DATA D3D11SubresourceData(const void* data, UINT pitch, UINT slicePitch)
{
	D3D11_SUBRESOURCE_DATA result;
	result.pSysMem = data;
	result.SysMemPitch = pitch;
	result.SysMemSlicePitch = slicePitch;
	return result;
}