#include "XEngine.GfxHAL.Common.h"

using namespace XEngine::GfxHAL;

bool XEngine::GfxHAL::ValidateTexelViewFormatValue(TexelViewFormat format)
{
	return format > TexelViewFormat::Undefined && format < TexelViewFormat::ValueCount;
}

bool XEngine::GfxHAL::DoesTextureFormatSupportColorRenderTargetUsage(TextureFormat format)
{
	// TODO: ...
	return true;
}

bool XEngine::GfxHAL::DoesTextureFormatSupportDepthRenderTargetUsage(TextureFormat format)
{
	// TODO: ...
	return true;
}

bool XEngine::GfxHAL::DoesTexelViewFormatSupportColorRenderTargetUsage(TexelViewFormat texelViewFormat)
{
	// TODO: ...
	return true;
}

bool XEngine::GfxHAL::DoesTexelViewFormatSupportVertexInputUsage(TexelViewFormat texelViewFormat)
{
	// TODO: ...
	return true;
}
