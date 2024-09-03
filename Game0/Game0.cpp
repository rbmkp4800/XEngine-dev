
#include <XLib.h>
#include <XLib.Vectors.h>
#include <XLib.Math.h>
#include <XLib.Math.Matrix4x4.h>
#include <XEngine.Gfx.HAL.D3D12.h>
#include <XEngine.Gfx.Allocation.h>
#include <XEngine.Gfx.ShaderLibraryLoader.h>
#include <XEngine.Gfx.Uploader.h>
#include <XEngine.Render.SceneRenderer.h>
#include <XEngine.System.h>

using namespace XLib;
using namespace XEngine;

class Game0 : public System::InputHandler
{
private:
	System::Window window;

	Gfx::HAL::Device gfxHwDevice;
	Gfx::HAL::CommandAllocatorHandle gfxHwCommandAllocator = {};
	Gfx::HAL::DescriptorAllocatorHandle gfxHwDescriptorAllocator = {};
	Gfx::HAL::OutputHandle gfxHwOutput = {};

	Gfx::CircularUploadMemoryAllocator gfxUploadMemoryAllocator;
	Gfx::Scheduler::TransientResourceCache gfxSchTransientResourceCache;
	Gfx::Scheduler::TaskGraph gfxSchTaskGraph;

	Render::Scene scene;
	Render::SceneRenderer sceneRenderer;

	uint16 outputWidth = 0;
	uint16 outputHeight = 0;
	float32x3 cameraPosition = {};
	float32x2 cameraRotation = {};
	float32 accum = 0.0f;

protected:
	virtual void onMouseMove(sint32 deltaX, sint32 deltaY) override;
	virtual void onMouseButton(System::MouseButton button, bool state) override {}
	virtual void onScroll(float32 delta) override {}
	virtual void onKeyboard(System::KeyCode key, bool state) override {}
	virtual void onCharacter(uint32 characterUTF32) override {}
	virtual void onCloseRequest() override {}

public:
	Game0() = default;
	~Game0() = default;

	void run();
};


////////////////////////////////////////////////////////////////////////////////////////////////////

void Game0::onMouseMove(sint32 deltaX, sint32 deltaY)
{
	cameraRotation.x += float32(deltaX) * 0.005f;
	cameraRotation.y -= float32(deltaY) * 0.005f;
}

void Game0::run()
{
	outputWidth = 1280;
	outputHeight = 720;

	window.create(outputWidth, outputHeight);

	System::RegisterInputHandler(this);
	//System::SetCursorEnabled(false);

	gfxHwDevice.initialize();
	gfxHwCommandAllocator = gfxHwDevice.createCommandAllocator();
	gfxHwDescriptorAllocator = gfxHwDevice.createDescriptorAllocator();
	gfxHwOutput = gfxHwDevice.createWindowOutput(outputWidth, outputHeight, window.getHandle());

	gfxUploadMemoryAllocator.initialize(gfxHwDevice, 16);
	gfxSchTransientResourceCache.initialize(gfxHwDevice);
	gfxSchTaskGraph.initialize();

	Gfx::GlobalUploader.initialize(gfxHwDevice);
	Gfx::GlobalShaderLibraryLoader.load("XEngine.Render.Shaders.xeslib", gfxHwDevice);

	scene.initialize(gfxHwDevice);
	sceneRenderer.initialize(gfxHwDevice);

	cameraPosition = { -13.0f, -7.0f, 10.0f };
	cameraRotation = { 0.0f, -0.7f };

	for (;;)
	{
		System::DispatchEvents();

		Render::CameraDesc cameraDesc = {};
		{
			float32x3 viewSpaceTranslation = { 0.0f, 0.0f, 0.0f };
			float32 translationStep = 0.1f;
			if (System::IsKeyDown(System::KeyCode('A')))
				viewSpaceTranslation.x -= translationStep;
			if (System::IsKeyDown(System::KeyCode('D')))
				viewSpaceTranslation.x += translationStep;

			if (System::IsKeyDown(System::KeyCode('Q')))
				viewSpaceTranslation.y -= translationStep;
			if (System::IsKeyDown(System::KeyCode('E')))
				viewSpaceTranslation.y += translationStep;

			if (System::IsKeyDown(System::KeyCode('W')))
				viewSpaceTranslation.z += translationStep;
			if (System::IsKeyDown(System::KeyCode('S')))
				viewSpaceTranslation.z -= translationStep;

			const float32x3 cameraUp = float32x3(0.0f, 0.0, 1.0f);
			const float32x3 cameraDirection = VectorMath::SphericalCoords_zZenith_xReference(cameraRotation);

			cameraPosition += cameraDirection * viewSpaceTranslation.z;
			cameraPosition.xy += float32x2(-Math::Sin(cameraRotation.x), Math::Cos(cameraRotation.x)) * viewSpaceTranslation.x;
			cameraPosition.z += viewSpaceTranslation.y;

			cameraDesc.position = cameraPosition;
			cameraDesc.direction = cameraDirection;
			cameraDesc.up = cameraUp;
			cameraDesc.fov = 1.0f;
			cameraDesc.zNear = 0.1f;
			cameraDesc.zFar = 100.0f;
		}

		gfxSchTaskGraph.open(gfxHwDevice);
		{
			const Gfx::HAL::TextureHandle gfxHwCurrentBackBuffer = gfxHwDevice.getOutputCurrentBackBuffer(gfxHwOutput);
			const Gfx::Scheduler::TextureHandle gfxSchCurrentBackBuffer =
				gfxSchTaskGraph.importExternalTexture(gfxHwCurrentBackBuffer,
					Gfx::HAL::TextureLayout::Present, Gfx::HAL::TextureLayout::Present);

			sceneRenderer.render(scene, cameraDesc, gfxSchTaskGraph, gfxSchCurrentBackBuffer, outputWidth, outputHeight);
		}
		gfxSchTaskGraph.execute(gfxSchTransientResourceCache, gfxHwCommandAllocator, gfxHwDescriptorAllocator, gfxUploadMemoryAllocator);

		const Gfx::HAL::DeviceQueueSyncPoint gfxHwFrameEndSyncPoint =
			gfxHwDevice.getEOPSyncPoint(Gfx::HAL::DeviceQueue::Graphics);

		gfxHwDevice.submitOutputFlip(Gfx::HAL::DeviceQueue::Graphics, gfxHwOutput);

		while (!gfxHwDevice.isQueueSyncPointReached(gfxHwFrameEndSyncPoint))
			{ }

		gfxHwDevice.resetCommandAllocator(gfxHwCommandAllocator);
		gfxHwDevice.resetDescriptorAllocator(gfxHwDescriptorAllocator);
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////

int __stdcall WinMain(void*, void*, char*, int)
{
	Game0 game0;
	game0.run();
	return 0;
}
