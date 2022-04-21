#include "XEngine.Render.HAL.Common.h"

using namespace XEngine::Render::HAL;

bool XEngine::Render::HAL::ValidateTexelViewFormatValue(TexelViewFormat format)
{
	return format > TexelViewFormat::Undefined && format < TexelViewFormat::ValueCount;
}

bool XEngine::Render::HAL::ValidateTexelViewFormatForRenderTargetUsage(TexelViewFormat format)
{
	// TODO:
	return true;
}
