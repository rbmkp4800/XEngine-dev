
#include <XLib.h>
#include <XLib.Vectors.h>
#include <XLib.Math.h>
#include <XLib.Math.Matrix4x4.h>
#include <XEngine.Gfx.HAL.D3D12.h>
#include <XEngine.Gfx.Allocation.h>
#include <XEngine.Gfx.ShaderLibraryLoader.h>
#include <XEngine.Gfx.Uploader.h>
#include <XEngine.Render.GeometryHeap.h>
#include <XEngine.Render.Scene.h>
#include <XEngine.Render.SceneRenderer.h>
#include <XEngine.Render.TextureHeap.h>
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

	Render::GeometryHandle rndTestCubeGeometry = {};
	Render::TextureHandle rndSampleAlbedoTexture = {};
	Render::TextureHandle rndSampleNrmTexture = {};

	Render::Scene rndScene;
	Render::SceneRenderer rndSceneRenderer;

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
	outputWidth = 1600;
	outputHeight = 900;

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

	Gfx::GUploader.initialize(gfxHwDevice);
	Gfx::GShaderLibraryLoader.load("XEngine.Render.Shaders.xeslib", gfxHwDevice);

	Render::GTextureHeap.initialize(gfxHwDevice);
	Render::GGeometryHeap.initialize(gfxHwDevice);

	rndTestCubeGeometry = Render::GGeometryHeap.createTestCube();
	rndSampleAlbedoTexture = Render::GTextureHeap.loadFromFile("../Content/wood-0.albedo.xerawtex");
	rndSampleNrmTexture = Render::GTextureHeap.loadFromFile("../Content/wood-0.nrm.xerawtex");

	rndScene.initialize(gfxHwDevice);
	rndSceneRenderer.initialize(gfxHwDevice);

	const Render::TransformSetHandle rndTransformSetA = rndScene.allocateTransformSet();
	const Render::TransformSetHandle rndTransformSetB = rndScene.allocateTransformSet();
	const Render::GeometryInstanceHandle rndGeometryInstanceA = rndScene.createGeometryInstance(rndTestCubeGeometry, rndTransformSetA);
	const Render::GeometryInstanceHandle rndGeometryInstanceB = rndScene.createGeometryInstance(rndTestCubeGeometry, rndTransformSetB);

	cameraPosition = { -4.0f, -4.0f, 3.0f };
	cameraRotation = { 0.785f, -0.4f };

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

		{
			static float32 aa = 0.0f;
			aa += 0.01f;

			rndScene.updateTransform(rndTransformSetA, 0, Matrix4x4::RotationZ(aa));
			rndScene.updateTransform(rndTransformSetB, 0, Matrix4x4::RotationZ(-aa) * Matrix4x4::Translation(0.0f, 0.0f, -3.0f));
		}

		{
			gfxSchTaskGraph.open(gfxHwDevice, gfxHwCommandAllocator, gfxHwDescriptorAllocator,
				gfxUploadMemoryAllocator, gfxSchTransientResourceCache);

			const Gfx::HAL::TextureHandle gfxHwCurrentBackBuffer = gfxHwDevice.getOutputCurrentBackBuffer(gfxHwOutput);
			const Gfx::Scheduler::TextureHandle gfxSchCurrentBackBuffer =
				gfxSchTaskGraph.importExternalTexture(gfxHwCurrentBackBuffer,
					Gfx::HAL::TextureLayout::Present, Gfx::HAL::TextureLayout::Present);

			rndSceneRenderer.render(rndScene, cameraDesc, gfxSchTaskGraph, gfxSchCurrentBackBuffer, outputWidth, outputHeight);

			gfxSchTaskGraph.execute();
		}

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
