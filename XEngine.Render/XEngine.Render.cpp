#include "XEngine.Render.h"

using namespace XEngine::Render;

HAL::Device Host::halDevice;

void Host::Initialize()
{
	HAL::Host::CreateDevice(halDevice);
}
