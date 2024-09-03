#include "XEngine.Gfx.HAL.Shared.h"

using namespace XEngine::Gfx::HAL;

bool TextureFormatUtils::SupportsColorRTUsage(TextureFormat format)
{
	switch (format)
	{
		case TextureFormat::R8:
		case TextureFormat::R8G8:
		case TextureFormat::R8G8B8A8:
		case TextureFormat::R16:
		case TextureFormat::R16G16:
		case TextureFormat::R16G16B16A16:
		case TextureFormat::R32:
		case TextureFormat::R32G32:
		case TextureFormat::R32G32B32:
		case TextureFormat::R32G32B32A32:
			return true;
	}
	return false;
}

bool TextureFormatUtils::SupportsDepthStencilRTUsage(TextureFormat format)
{
	return TranslateToDepthStencilFormat(format) != DepthStencilFormat::Undefined;
}

DepthStencilFormat TextureFormatUtils::TranslateToDepthStencilFormat(TextureFormat format)
{
	switch (format)
	{
		case TextureFormat::D16:	return DepthStencilFormat::D16;
		case TextureFormat::D32:	return DepthStencilFormat::D32;
		case TextureFormat::D24S8:	return DepthStencilFormat::D24S8;
		case TextureFormat::D32S8:	return DepthStencilFormat::D32S8;
	}
	return DepthStencilFormat::Undefined;
}

uint8 TextureFormatUtils::GetTexelByteSize(TextureFormat format)
{
	switch (format)
	{
		case TextureFormat::R8:
			return 1;

		case TextureFormat::R8G8:
		case TextureFormat::R16:
		case TextureFormat::D16:
			return 2;

		case TextureFormat::R8G8B8A8:
		case TextureFormat::R16G16:
		case TextureFormat::R32:
		case TextureFormat::D32:
			return 4;

		case TextureFormat::R16G16B16A16:
		case TextureFormat::R32G32:
			return 8;

		case TextureFormat::R32G32B32:
			return 12;

		case TextureFormat::R32G32B32A32:
			return 16;
		
		case TextureFormat::D24S8:
		case TextureFormat::D32S8:
			// Multi-planar formats are not allowed.
			XAssertUnreachableCode();
			return 0;

			// Block formats are not allowed.
	}

	XAssertUnreachableCode();
	return 0;
}

bool TexelViewFormatUtils::SupportsColorRTUsage(TexelViewFormat format)
{
	XTODO(__FUNCTION__ " not implemented");
	return true;
}

bool TexelViewFormatUtils::SupportsVertexInputUsage(TexelViewFormat format)
{
	XTODO(__FUNCTION__ " not implemented");
	return true;
}

/*uint8 TexelViewFormatUtils::GetByteSize(TexelViewFormat format)
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
}*/
