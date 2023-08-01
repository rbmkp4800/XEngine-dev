#include <XLib.h>
#include <XLib.Math.h>

#include <XEngine.Gfx.HAL.D3D12.h>
#include <XEngine.Gfx.ShaderLibrary.h>
#include <XEngine.System.h>

using namespace XLib;
using namespace XEngine;

class Game0 : public System::InputHandler
{
private:
	System::Window window;

	GfxHAL::Device gfxDevice;
	GfxHAL::CommandList gfxCommandList;
	GfxHAL::SurfaceHandle gfxSurface = GfxHAL::SurfaceHandle::Zero;
	GfxHAL::TextureHandle gfxSurfaceBuffers[GfxHAL::SurfaceBackBufferCount] = {};
	GfxHAL::RenderTargetViewHandle gfxSurfaceBufferRTVs[GfxHAL::SurfaceBackBufferCount] = {};

	Gfx::ShaderLibrary gfxShaderLibrary;

protected:
	virtual void onMouseMove(sint32 xDelta, sint32 yDelta) override {}
	virtual void onMouseButton(System::MouseButton button, bool state) override {}
	virtual void onScroll(float32 xDelta, float32 yDelta) override {}
	virtual void onKeyboard(System::KeyCode key, bool state) override {}
	virtual void onCharacter(uint32 characterUTF32) override {}
	virtual void onCloseRequest() override {}

public:
	Game0() = default;
	~Game0() = default;

	void run();
};

void Game0::run()
{
	window.create(1600, 900);

	//System::RegisterInputHandler(this);
	//System::SetCursorEnabled(false);

	GfxHAL::Device::Create(gfxDevice);
	gfxDevice.createCommandList(gfxCommandList, GfxHAL::CommandListType::Graphics);

	gfxSurface = gfxDevice.createWindowSurface(1600, 900, window.getHandle());
	for (int i = 0; i < GfxHAL::SurfaceBackBufferCount; i++)
	{
		gfxSurfaceBuffers[i] = gfxDevice.getSurfaceBackBuffer(gfxSurface, i);
		gfxSurfaceBufferRTVs[i] = gfxDevice.createRenderTargetView(gfxSurfaceBuffers[i], GfxHAL::TexelViewFormat::R8G8B8A8_UNORM);
	}

	gfxShaderLibrary.load("../Build/XEngine.Render.Shaders.xeslib", gfxDevice);

	for (;;)
	{
		System::DispatchEvents();

		const uint32 currentBackBufferIndex = gfxDevice.getSurfaceCurrentBackBufferIndex(gfxSurface);
		const GfxHAL::TextureHandle gfxCurrentBackBuffer = gfxSurfaceBuffers[currentBackBufferIndex];
		const GfxHAL::RenderTargetViewHandle gfxCurrentRTV = gfxSurfaceBufferRTVs[currentBackBufferIndex];

		gfxCommandList.open();
		gfxCommandList.textureMemoryBarrier(gfxCurrentBackBuffer,
			GfxHAL::BarrierSync::None, GfxHAL::BarrierSync::All,
			GfxHAL::BarrierAccess::None, GfxHAL::BarrierAccess::RenderTarget,
			GfxHAL::TextureLayout::Present, GfxHAL::TextureLayout::RenderTarget);

		static float qqq = 0.0f;
		qqq += 0.1f;

		float32 color[4] = { Math::Sin(qqq) * 0.5f + 0.5f, Math::Sin(qqq + 1.0f) * 0.5f + 0.5f, 0.0f, 1.0f };
		gfxCommandList.clearRenderTarget(gfxCurrentRTV, color);

		gfxCommandList.textureMemoryBarrier(gfxCurrentBackBuffer,
			GfxHAL::BarrierSync::All, GfxHAL::BarrierSync::None,
			GfxHAL::BarrierAccess::RenderTarget, GfxHAL::BarrierAccess::None,
			GfxHAL::TextureLayout::RenderTarget, GfxHAL::TextureLayout::Present);

		gfxDevice.submitWorkload(GfxHAL::DeviceQueue::Main, gfxCommandList);
		gfxDevice.submitSurfaceFlip(gfxSurface);

		const GfxHAL::DeviceQueueSyncPoint gfxEndOfQueueSyncPoint = gfxDevice.getEndOfQueueSyncPoint(GfxHAL::DeviceQueue::Main);
		while (!gfxDevice.isQueueSyncPointReached(gfxEndOfQueueSyncPoint))
			{ }
	}
}

int __stdcall WinMain(void*, void*, char*, int)
{
	Game0 game0;
	game0.run();
	return 0;
}
