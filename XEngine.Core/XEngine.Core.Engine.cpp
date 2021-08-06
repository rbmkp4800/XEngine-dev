#include <XEngine.Render.Device.h>
#include <XEngine.Render.Target.h>
#include <XEngine.ThreadPool.h>

#include "XEngine.Core.Engine.h"

#include "XEngine.Core.Input.h"
#include "XEngine.Core.Output.h"

using namespace XLib;
using namespace XEngine;
using namespace XEngine::Core;

static bool running = false;
static Render::Device renderDevice;
static Render::Output imageOutput;

void Engine::Run(GameBase* game)
{
	running = true;

	//XEngine::ThreadPool::Run(...);

	Output::Initialize();
	renderDevice.initialize();

	game->initialize();

	imageOutput.initialize(renderDevice,
		Output::GetViewWindowHandle(0), Output::GetViewResolution(0));

	while (running)
	{
		if (imageOutput.getSize() != Output::GetViewResolution(0))
			imageOutput.resize(Output::GetViewResolution(0));

		game->update(0.0f);

		renderDevice.graphicsQueueOutputFlip(imageOutput);
	}

	Output::Destroy();
}

void Engine::Shutdown()
{
	running = false;
}

Render::Device& Engine::GetRenderDevice() { return renderDevice; }

uint32 Engine::GetOutputViewCount() { return 1; }
Render::Target& Engine::GetOutputViewRenderTarget(uint32 outputViewIndex) { return imageOutput.getCurrentTarget(); }
uint16x2 Engine::GetOutputViewRenderTargetSize(uint32 outputViewIndex) { return imageOutput.getSize(); }