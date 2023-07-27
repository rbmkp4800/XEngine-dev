#include "XEngine.GfxHAL.Common.h"

using namespace XEngine::GfxHAL;

bool TextureFormatUtils::SupportsRenderTargetUsage(TextureFormat format)
{
	XTODO(__FUNCTION__ " not implemented");
	return true;
}

bool TextureFormatUtils::SupportsDepthStencilUsage(TextureFormat format)
{
	XTODO(__FUNCTION__ " not implemented");
	return true;
}

bool TexelViewFormatUtils::SupportsRenderTargetUsage(TexelViewFormat format)
{
	XTODO(__FUNCTION__ " not implemented");
	return true;
}

bool TexelViewFormatUtils::SupportsVertexInputUsage(TexelViewFormat format)
{
	XTODO(__FUNCTION__ " not implemented");
	return true;
}

uint8 TexelViewFormatUtils::GetByteSize(TexelViewFormat format)
{
	switch (format)
	{
		case TexelViewFormat::R8_UNORM:
		case TexelViewFormat::R8_SNORM:
		case TexelViewFormat::R8_UINT:
		case TexelViewFormat::R8_SINT:
			return 1;

		case TexelViewFormat::R8G8_UNORM:
		case TexelViewFormat::R8G8_SNORM:
		case TexelViewFormat::R8G8_UINT:
		case TexelViewFormat::R8G8_SINT:
		case TexelViewFormat::R16_FLOAT:
		case TexelViewFormat::R16_UNORM:
		case TexelViewFormat::R16_SNORM:
		case TexelViewFormat::R16_UINT:
		case TexelViewFormat::R16_SINT:
			return 2;

		case TexelViewFormat::R8G8B8A8_UNORM:
		case TexelViewFormat::R8G8B8A8_SNORM:
		case TexelViewFormat::R8G8B8A8_UINT:
		case TexelViewFormat::R8G8B8A8_SINT:
		case TexelViewFormat::R8G8B8A8_SRGB:
		case TexelViewFormat::R16G16_FLOAT:
		case TexelViewFormat::R16G16_UNORM:
		case TexelViewFormat::R16G16_SNORM:
		case TexelViewFormat::R16G16_UINT:
		case TexelViewFormat::R16G16_SINT:
		case TexelViewFormat::R32_FLOAT:
		case TexelViewFormat::R32_UNORM:
		case TexelViewFormat::R32_SNORM:
		case TexelViewFormat::R32_UINT:
		case TexelViewFormat::R32_SINT:
			return 4;

		case TexelViewFormat::R16G16B16A16_FLOAT:
		case TexelViewFormat::R16G16B16A16_UNORM:
		case TexelViewFormat::R16G16B16A16_SNORM:
		case TexelViewFormat::R16G16B16A16_UINT:
		case TexelViewFormat::R16G16B16A16_SINT:
		case TexelViewFormat::R32G32_FLOAT:
		case TexelViewFormat::R32G32_UINT:
		case TexelViewFormat::R32G32_SINT:
			return 8;

		case TexelViewFormat::R32G32B32_FLOAT:
		case TexelViewFormat::R32G32B32_UINT:
		case TexelViewFormat::R32G32B32_SINT:
			return 12;

		case TexelViewFormat::R32G32B32A32_FLOAT:
		case TexelViewFormat::R32G32B32A32_UINT:
		case TexelViewFormat::R32G32B32A32_SINT:
			return 16;
	}

	XAssertUnreachableCode();
	return 0;
}
