#include <XLib.h>

#include <XEngine.Gfx.HAL.D3D12.h>
#include <XEngine.Gfx.ShaderLibrary.h>
#include <XEngine.System.h>

using namespace XLib;
using namespace XEngine;

class Game0 : public System::InputHandler
{
private:
	System::OutputSurface outputSurface;
	Gfx::HAL::Device gfxDevice;
	Gfx::HAL::SwapChainHandle gfxSwapChain = Gfx::HAL::SwapChainHandle::Zero;
	Gfx::ShaderLibrary gfxShaderLibrary;

protected:
	virtual void onMouseMove(sint32 xDelta, sint32 yDelta) override;
	virtual void onMouseButton(System::MouseButton button, bool state) override;
	virtual void onScroll(float32 xDelta, float32 yDelta) override;
	virtual void onKeyboard(System::KeyCode key, bool state) override;
	virtual void onCharacter(uint32 characterUTF32) override;
	virtual void onCloseRequest() override;

public:
	Game0() = default;
	~Game0() = default;

	void run();
};

void Game0::run()
{
	System::CreateWindowOutputSurface(1600, 900, outputSurface);
	System::RegisterInputHandler(this);
	System::SetCursorEnabled(false);

	Gfx::HAL::Device::Create(gfxDevice);
	gfxShaderLibrary.load("../Build/XEngine.Render.Shaders.xeslib", gfxDevice);

	gfxDevice.createSwapChain(1600, 900, outputSurface);

	for (;;)
	{


		System::DispatchEvents();
	}
}

int __stdcall WinMain(void*, void*, char*, int)
{
	Game0 game0;
	game0.run();
	return 0;
}
