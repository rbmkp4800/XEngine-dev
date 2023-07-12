#include "XEngine.Render.h"

using namespace XEngine;
using namespace XEngine::Render;

GfxHAL::Device Host::halDevice;
ShaderLibrary Host::shaderLibrary;

void Host::Initialize()
{
	GfxHAL::Host::CreateDevice(halDevice);

	// TODO: `shaderLibrary.load` assumes that `Render::Host` is fully initialized.
	shaderLibrary.load("../Build/XEngine.Render.Shaders.xeslib");
}
