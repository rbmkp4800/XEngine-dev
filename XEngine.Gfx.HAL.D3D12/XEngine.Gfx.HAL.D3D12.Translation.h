#pragma once

// NOTE: This file should not be included directly by HAL users.
// Assumes that d3d12 and dxgi are included above.

namespace XEngine::Gfx::HAL
{
	inline DXGI_FORMAT TranslateTextureFormatToDXGIFormat(TextureFormat format)
	{
		switch (format)
		{
			case TextureFormat::Undefined:		return DXGI_FORMAT_UNKNOWN;
			case TextureFormat::R8:				return DXGI_FORMAT_R8_TYPELESS;
			case TextureFormat::R8G8:			return DXGI_FORMAT_R8G8_TYPELESS;
			case TextureFormat::R8G8B8A8:		return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			case TextureFormat::R16:			return DXGI_FORMAT_R16_TYPELESS;
			case TextureFormat::R16G16:			return DXGI_FORMAT_R16G16_TYPELESS;
			case TextureFormat::R16G16B16A16:	return DXGI_FORMAT_R16G16B16A16_TYPELESS;
			case TextureFormat::R32:			return DXGI_FORMAT_R32_TYPELESS;
			case TextureFormat::R32G32:			return DXGI_FORMAT_R32G32_TYPELESS;
			case TextureFormat::R32G32B32:		return DXGI_FORMAT_R32G32B32_TYPELESS;
			case TextureFormat::R32G32B32A32:	return DXGI_FORMAT_R32G32B32A32_TYPELESS;
			case TextureFormat::D16:			return DXGI_FORMAT_D16_UNORM;
			case TextureFormat::D32:			return DXGI_FORMAT_D32_FLOAT;
			case TextureFormat::D24S8:			return DXGI_FORMAT_D24_UNORM_S8_UINT;
			case TextureFormat::D32S8:			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		}

		XEMasterAssertUnreachableCode();
		return DXGI_FORMAT_UNKNOWN;
	}

	inline DXGI_FORMAT TranslateTexelViewFormatToDXGIFormat(TexelViewFormat format)
	{
		switch (format)
		{
			case TexelViewFormat::R8_UNORM:				return DXGI_FORMAT_R8_UNORM;
			case TexelViewFormat::R8_SNORM:				return DXGI_FORMAT_R8_SNORM;
			case TexelViewFormat::R8_UINT:				return DXGI_FORMAT_R8_UINT;
			case TexelViewFormat::R8_SINT:				return DXGI_FORMAT_R8_SINT;
			case TexelViewFormat::R8G8_UNORM:			return DXGI_FORMAT_R8G8_UNORM;
			case TexelViewFormat::R8G8_SNORM:			return DXGI_FORMAT_R8G8_SNORM;
			case TexelViewFormat::R8G8_UINT:			return DXGI_FORMAT_R8G8_UINT;
			case TexelViewFormat::R8G8_SINT:			return DXGI_FORMAT_R8G8_SINT;
			case TexelViewFormat::R8G8B8A8_UNORM:		return DXGI_FORMAT_R8G8B8A8_UNORM;
			case TexelViewFormat::R8G8B8A8_SNORM:		return DXGI_FORMAT_R8G8B8A8_SNORM;
			case TexelViewFormat::R8G8B8A8_UINT:		return DXGI_FORMAT_R8G8B8A8_UINT;
			case TexelViewFormat::R8G8B8A8_SINT:		return DXGI_FORMAT_R8G8B8A8_SINT;
			case TexelViewFormat::R8G8B8A8_SRGB:		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case TexelViewFormat::R16_FLOAT:			return DXGI_FORMAT_R16_FLOAT;
			case TexelViewFormat::R16_UNORM:			return DXGI_FORMAT_R16_UNORM;
			case TexelViewFormat::R16_SNORM:			return DXGI_FORMAT_R16_SNORM;
			case TexelViewFormat::R16_UINT:				return DXGI_FORMAT_R16_UINT;
			case TexelViewFormat::R16_SINT:				return DXGI_FORMAT_R16_SINT;
			case TexelViewFormat::R16G16_FLOAT:			return DXGI_FORMAT_R16G16_FLOAT;
			case TexelViewFormat::R16G16_UNORM:			return DXGI_FORMAT_R16G16_UNORM;
			case TexelViewFormat::R16G16_SNORM:			return DXGI_FORMAT_R16G16_SNORM;
			case TexelViewFormat::R16G16_UINT:			return DXGI_FORMAT_R16G16_UINT;
			case TexelViewFormat::R16G16_SINT:			return DXGI_FORMAT_R16G16_SINT;
			case TexelViewFormat::R16G16B16A16_FLOAT:	return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case TexelViewFormat::R16G16B16A16_UNORM:	return DXGI_FORMAT_R16G16B16A16_UNORM;
			case TexelViewFormat::R16G16B16A16_SNORM:	return DXGI_FORMAT_R16G16B16A16_SNORM;
			case TexelViewFormat::R16G16B16A16_UINT:	return DXGI_FORMAT_R16G16B16A16_UINT;
			case TexelViewFormat::R16G16B16A16_SINT:	return DXGI_FORMAT_R16G16B16A16_SINT;
			case TexelViewFormat::R32_FLOAT:			return DXGI_FORMAT_R32_FLOAT;
			case TexelViewFormat::R32_UINT:				return DXGI_FORMAT_R32_UINT;
			case TexelViewFormat::R32_SINT:				return DXGI_FORMAT_R32_SINT;
			case TexelViewFormat::R32G32_FLOAT:			return DXGI_FORMAT_R32G32_FLOAT;
			case TexelViewFormat::R32G32_UINT:			return DXGI_FORMAT_R32G32_UINT;
			case TexelViewFormat::R32G32_SINT:			return DXGI_FORMAT_R32G32_SINT;
			case TexelViewFormat::R32G32B32_FLOAT:		return DXGI_FORMAT_R32G32B32_FLOAT;
			case TexelViewFormat::R32G32B32_UINT:		return DXGI_FORMAT_R32G32B32_UINT;
			case TexelViewFormat::R32G32B32_SINT:		return DXGI_FORMAT_R32G32B32_SINT;
			case TexelViewFormat::R32G32B32A32_FLOAT:	return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case TexelViewFormat::R32G32B32A32_UINT:	return DXGI_FORMAT_R32G32B32A32_UINT;
			case TexelViewFormat::R32G32B32A32_SINT:	return DXGI_FORMAT_R32G32B32A32_SINT;

		}

		XEMasterAssertUnreachableCode();
		return DXGI_FORMAT_UNKNOWN;
	}

	inline DXGI_FORMAT TranslateDepthStencilFormatToDXGIFormat(DepthStencilFormat format)
	{
		switch (format)
		{
			case DepthStencilFormat::Undefined:	return DXGI_FORMAT_UNKNOWN;
			case DepthStencilFormat::D16:		return DXGI_FORMAT_D16_UNORM;
			case DepthStencilFormat::D32:		return DXGI_FORMAT_D32_FLOAT;
			case DepthStencilFormat::D24S8:		return DXGI_FORMAT_D24_UNORM_S8_UINT;
			case DepthStencilFormat::D32S8:		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
		}

		XEMasterAssertUnreachableCode();
		return DXGI_FORMAT_UNKNOWN;
	}

	inline D3D12_BARRIER_SYNC TranslateBarrierSyncToD3D12BarrierSync(BarrierSync sync)
	{
		if (sync == BarrierSync::All)
			return D3D12_BARRIER_SYNC_ALL;

		D3D12_BARRIER_SYNC d3dBarrierSync = D3D12_BARRIER_SYNC(0);

		if ((sync & BarrierSync::AllGraphics) == BarrierSync::AllGraphics)
		{
			d3dBarrierSync |= D3D12_BARRIER_SYNC_DRAW;
			sync ^= BarrierSync::AllGraphics;
		}

		if ((sync & BarrierSync::AllShaders) == BarrierSync::AllShaders)
		{
			d3dBarrierSync |= D3D12_BARRIER_SYNC_ALL_SHADING;
			sync ^= BarrierSync::AllShaders;
		}

		if ((sync & BarrierSync::Copy) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_COPY;
		if ((sync & BarrierSync::ComputeShader) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		if ((sync & BarrierSync::PrePixelShaders) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_VERTEX_SHADING;
		if ((sync & BarrierSync::PixelShader) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_PIXEL_SHADING;
		if ((sync & BarrierSync::ColorRenderTarget) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_RENDER_TARGET;
		if ((sync & BarrierSync::DepthStencilRenderTarget) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		if ((sync & BarrierSync::RaytracingAccelerationStructureBuild) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
		if ((sync & BarrierSync::RaytracingAccelerationStructureCopy) != BarrierSync(0))
			d3dBarrierSync |= D3D12_BARRIER_SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE;

		return d3dBarrierSync;
	}

	inline D3D12_BARRIER_ACCESS TranslateBarrierAccessToD3D12BarrierAccess(BarrierAccess access)
	{
		if (access == BarrierAccess::None)
			return D3D12_BARRIER_ACCESS_NO_ACCESS;
		if (access == BarrierAccess::Any)
			return D3D12_BARRIER_ACCESS_COMMON;

		D3D12_BARRIER_ACCESS d3dBarrierAccess = D3D12_BARRIER_ACCESS(0);

		if ((access & BarrierAccess::CopySource) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
		if ((access & BarrierAccess::CopyDest) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_COPY_DEST;
		if ((access & BarrierAccess::VertexOrIndexBuffer) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_INDEX_BUFFER | D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
		if ((access & BarrierAccess::ConstantBuffer) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
		if ((access & BarrierAccess::ShaderReadOnly) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		if ((access & BarrierAccess::ShaderReadWrite) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		if ((access & BarrierAccess::ColorRenderTarget) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
		if ((access & BarrierAccess::DepthStencilRenderTarget) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		if ((access & BarrierAccess::DepthStencilRenderTargetReadOnly) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
		if ((access & BarrierAccess::RaytracingAccelerationStructureRead) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
		if ((access & BarrierAccess::RaytracingAccelerationStructureWrite) != BarrierAccess(0))
			d3dBarrierAccess |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;

		return d3dBarrierAccess;
	}

	inline D3D12_BARRIER_LAYOUT TranslateTextureLayoutToD3D12BarrierLayout(TextureLayout textureLayout)
	{
		switch (textureLayout)
		{
			case TextureLayout::Undefined:							return D3D12_BARRIER_LAYOUT_UNDEFINED;
			case TextureLayout::Present:							return D3D12_BARRIER_LAYOUT_PRESENT;
			case TextureLayout::Common:								return D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON;
			case TextureLayout::ShaderReadOnly:						return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
			case TextureLayout::ShaderReadWrite:					return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
			case TextureLayout::ColorRenderTarget:					return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
			case TextureLayout::DepthStencilRenderTarget:			return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
			case TextureLayout::DepthStencilRenderTargetReadOnly:	return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
		}

		// TODO: Maybe we want to use `D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_*` and `D3D12_BARRIER_LAYOUT_COMPUTE_QUEUE_*` ?

		XEMasterAssertUnreachableCode();
		return D3D12_BARRIER_LAYOUT_UNDEFINED;
	}

	inline D3D12_FILL_MODE TranslateRasterizerFillModeToD3D12FillMode(RasterizerFillMode rasterizerFillMode)
	{
		switch (rasterizerFillMode)
		{
			case RasterizerFillMode::Solid:		return D3D12_FILL_MODE_SOLID;
			case RasterizerFillMode::Wireframe:	return D3D12_FILL_MODE_WIREFRAME;
		}

		XEMasterAssertUnreachableCode();
		return D3D12_FILL_MODE_SOLID;
	}

	inline D3D12_CULL_MODE TranslateRasterizerCullModeToD3D12CullMode(RasterizerCullMode rasterizerCullMode)
	{
		switch (rasterizerCullMode)
		{
			case RasterizerCullMode::None:	return D3D12_CULL_MODE_NONE;
			case RasterizerCullMode::Front:	return D3D12_CULL_MODE_FRONT;
			case RasterizerCullMode::Back:	return D3D12_CULL_MODE_BACK;
		}

		XEMasterAssertUnreachableCode();
		return D3D12_CULL_MODE_NONE;
	}

	inline D3D12_COMPARISON_FUNC TranslateComparisonFuncToD3D12ComparisonFunc(ComparisonFunc comparisonFunc)
	{
		switch (comparisonFunc)
		{
			case ComparisonFunc::Never:			return D3D12_COMPARISON_FUNC_NEVER;			break;
			case ComparisonFunc::Always:		return D3D12_COMPARISON_FUNC_ALWAYS;		break;
			case ComparisonFunc::Equal:			return D3D12_COMPARISON_FUNC_EQUAL;			break;
			case ComparisonFunc::NotEqual:		return D3D12_COMPARISON_FUNC_NOT_EQUAL;		break;
			case ComparisonFunc::Less:			return D3D12_COMPARISON_FUNC_LESS;			break;
			case ComparisonFunc::LessEqual:		return D3D12_COMPARISON_FUNC_LESS_EQUAL;	break;
			case ComparisonFunc::Greater:		return D3D12_COMPARISON_FUNC_GREATER;		break;
			case ComparisonFunc::GreaterEqual:	return D3D12_COMPARISON_FUNC_GREATER_EQUAL;	break;
		}

		XEMasterAssertUnreachableCode();
		return D3D12_COMPARISON_FUNC_NEVER;
	}

	inline D3D12_BOX D3D12BoxFromOffsetAndSize(uint16x3 offset, uint16x3 size)
	{
		D3D12_BOX d3dBox = {};
		d3dBox.left = offset.x;
		d3dBox.top = offset.y;
		d3dBox.front = offset.z;
		d3dBox.right = offset.x + size.x;
		d3dBox.bottom = offset.y + size.y;
		d3dBox.back = offset.z + size.z;
		return d3dBox;
	}

	inline D3D12_RESOURCE_DESC1 TranslateTextureDescToD3D12ResourceDesc1(TextureDesc textureDesc)
	{
		XEAssert(textureDesc.dimension == TextureDimension::Texture2D);

		D3D12_RESOURCE_DESC1 d3dResourceDesc = {};
		d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		d3dResourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		d3dResourceDesc.Width = textureDesc.size.x;
		d3dResourceDesc.Height = textureDesc.size.y;
		d3dResourceDesc.DepthOrArraySize = 1;
		d3dResourceDesc.MipLevels = textureDesc.mipLevelCount;
		d3dResourceDesc.Format = TranslateTextureFormatToDXGIFormat(textureDesc.format);
		d3dResourceDesc.SampleDesc.Count = 1;
		d3dResourceDesc.SampleDesc.Quality = 0;
		d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (TextureFormatUtils::SupportsDepthStencilRTUsage(textureDesc.format))
			d3dResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		else
		{
			d3dResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			if (textureDesc.enableRenderTargetUsage)
			{
				XEAssert(TextureFormatUtils::SupportsColorRTUsage(textureDesc.format));
				d3dResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			}
		}

		return d3dResourceDesc;
	}
}
