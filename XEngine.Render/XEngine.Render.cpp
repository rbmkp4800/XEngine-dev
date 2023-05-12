#include "XEngine.Render.h"

using namespace XEngine::Render;

HAL::Device Host::halDevice;
ShaderLibrary Host::shaderLibrary;

void Host::Initialize()
{
	HAL::Host::CreateDevice(halDevice);

	// TODO: `shaderLibrary.load` assumes that `Render::Host` is fully initialized.
	shaderLibrary.load("../Build/shaderpack");
}
