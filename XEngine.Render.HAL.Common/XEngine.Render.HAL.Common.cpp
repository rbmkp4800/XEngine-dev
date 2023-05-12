#include "XEngine.Render.HAL.Common.h"

using namespace XEngine::Render::HAL;

bool XEngine::Render::HAL::ValidateTexelViewFormatValue(TexelViewFormat format)
{
	return format > TexelViewFormat::Undefined && format < TexelViewFormat::ValueCount;
}

bool XEngine::Render::HAL::DoesTextureFormatSupportColorRenderTargetUsage(TextureFormat format)
{
	// TODO: ...
	return true;
}

bool XEngine::Render::HAL::DoesTextureFormatSupportDepthRenderTargetUsage(TextureFormat format)
{
	// TODO: ...
	return true;
}

bool XEngine::Render::HAL::DoesTexelViewFormatSupportColorRenderTargetUsage(TexelViewFormat texelViewFormat)
{
	// TODO: ...
	return true;
}
