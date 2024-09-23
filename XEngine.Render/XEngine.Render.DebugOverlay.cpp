#include <XEngine.Gfx.HAL.D3D12.h>

#include "XEngine.Render.DebugOverlay.h"

using namespace XEngine::Gfx;
using namespace XEngine::Render;

struct VertexColor
{
	float32x2 position;
	uint32 color;
};

struct VertexText
{
	float32x2 position;
	uint32 color;
	uint16x2 uv;
};

struct GlobalConstantBuffer
{
	float32x2 positionScaler;
};

struct FontConstantBuffer
{
	float32x2 uvScaler;
	uint32 fontTextureIndex;
};
