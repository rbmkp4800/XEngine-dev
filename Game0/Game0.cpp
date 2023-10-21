#include <XLib.h>
#include <XLib.Vectors.h>
#include <XLib.Math.h>
#include <XLib.Math.Matrix4x4.h>

#include <XEngine.XStringHash.h>
#include <XEngine.Gfx.HAL.D3D12.h>
#include <XEngine.Gfx.ShaderLibrary.h>
#include <XEngine.Gfx.Allocation.h>
#include <XEngine.System.h>

using namespace XLib;
using namespace XEngine;

struct TestVertex
{
	float32x3 position;
	float32x3 normal;
	float32x2 texcoord;
};

TestVertex cubeVertices[] =
{
	{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f } },
	{ {  1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f } },
	{ {  1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f } },
	{ { -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f } },

	{ { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f } },
	{ {  1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f } },
	{ {  1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f } },
	{ { -1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f } },

	{ { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
	{ {  1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 0.0f } },
	{ { -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 0.0f } },
	{ {  1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },

	{ { -1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f } },
	{ {  1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f, 0.0f } },
	{ {  1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f } },
	{ { -1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f, 0.0f } },

	{ { -1.0f, -1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f } },
	{ { -1.0f,  1.0f,  1.0f }, { -1.0f, 0.0f, 0.0f } },
	{ { -1.0f, -1.0f,  1.0f }, { -1.0f, 0.0f, 0.0f } },
	{ { -1.0f,  1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f } },

	{ {  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } },
	{ {  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f, 0.0f } },
	{ {  1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } },
	{ {  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f, 0.0f } },
};

uint16 cubeIndices[] =
{
	 0,  1,  2,
	 0,  3,  1,

	 4,  6,  5,
	 4,  5,  7,

	 8, 10,  9,
	 8,  9, 11,

	12, 14, 13,
	12, 13, 15,

	16, 18, 17,
	16, 17, 19,

	20, 22, 21,
	20, 21, 23,
};

class Game0 : public System::InputHandler
{
private:
	System::Window window;

	GfxHAL::Device gfxDevice;
	GfxHAL::CommandAllocatorHandle gfxCommandAllocator;
	GfxHAL::OutputHandle gfxOutput = GfxHAL::OutputHandle::Zero;
	GfxHAL::ColorRenderTargetHandle gfxOutputBackBuffersColorRTs[GfxHAL::OutputBackBufferCount] = {};
	GfxHAL::BufferHandle gfxTestBuffer = GfxHAL::BufferHandle::Zero;
	void* mappedTestBuffer = nullptr;

	Gfx::ShaderLibrary gfxShaderLibrary;
	Gfx::TransientUploadMemoryAllocator gfxTransientAllocator;

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
	window.create(1600, 900);

	System::RegisterInputHandler(this);
	//System::SetCursorEnabled(false);

	GfxHAL::DeviceSettings gfxDeviceSettings = {};

	gfxDeviceSettings.maxMemoryAllocationCount = 1024;
	gfxDeviceSettings.maxCommandAllocatorCount = 8;
	gfxDeviceSettings.maxCommandListCount = 8;
	gfxDeviceSettings.maxResourceCount = 1024;
	gfxDeviceSettings.maxResourceViewCount = 1024;
	gfxDeviceSettings.maxColorRenderTargetCount = 64;
	gfxDeviceSettings.maxDepthStencilRenderTargetCount = 64;
	gfxDeviceSettings.maxDescriptorSetLayoutCount = 64;
	gfxDeviceSettings.maxPipelineLayoutCount = 64;
	gfxDeviceSettings.maxPipelineCount = 64;
	gfxDeviceSettings.maxOutputCount = 4;
	gfxDeviceSettings.descriptorPoolSize = 1024;

	gfxDevice.initialize(gfxDeviceSettings);
	gfxCommandAllocator = gfxDevice.createCommandAllocator();
	gfxTransientAllocator.initialize(gfxDevice, 16);

	gfxOutput = gfxDevice.createWindowOutput(1600, 900, window.getHandle());
	for (int i = 0; i < GfxHAL::OutputBackBufferCount; i++)
	{
		GfxHAL::TextureHandle gfxBackBuffer = gfxDevice.getOutputBackBuffer(gfxOutput, i);
		gfxOutputBackBuffersColorRTs[i] = gfxDevice.createColorRenderTarget(gfxBackBuffer, GfxHAL::TexelViewFormat::R8G8B8A8_UNORM);
	}

	gfxTestBuffer = gfxDevice.createStagingBuffer(64 * 1024, GfxHAL::StagingBufferAccessMode::DeviceReadHostWrite);
	mappedTestBuffer = gfxDevice.getMappedBufferPtr(gfxTestBuffer);

	TestVertex* vertexBuffer = (TestVertex*)mappedTestBuffer;
	uint16* indexBuffer = (uint16*)(uintptr(mappedTestBuffer) + 4096);

	memoryCopy(vertexBuffer, cubeVertices, sizeof(cubeVertices));
	memoryCopy(indexBuffer, cubeIndices, sizeof(cubeIndices));

	gfxShaderLibrary.load("XEngine.Render.Shaders.xeslib", gfxDevice);

	const GfxHAL::PipelineLayoutHandle gfxTestPL = gfxShaderLibrary.getPipelineLayout("Test.PipelineLayout"_xsh);
	const GfxHAL::PipelineHandle gfxTestGfxPipeline = gfxShaderLibrary.getPipeline("Test.GraphicsPipeline"_xsh);

	cameraPosition = { -13.0f, -7.0f, 10.0f };
	cameraRotation = { 0.0f, -0.7f };

	for (;;)
	{
		System::DispatchEvents();

		accum += 0.01f;

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

		const Matrix4x4 cameraViewMatrix = Matrix4x4::LookAtCentered(cameraPosition, cameraDirection, cameraUp);
		const Matrix4x4 cameraProjectionMatrix = Matrix4x4::Perspective(1.0f, 16.0f / 9.0f, 0.1f, 100.0f);

		const uint32 currentBackBufferIndex = gfxDevice.getOutputCurrentBackBufferIndex(gfxOutput);
		const GfxHAL::TextureHandle gfxCurrentBackBuffer = gfxDevice.getOutputBackBuffer(gfxOutput, currentBackBufferIndex);
		const GfxHAL::ColorRenderTargetHandle gfxCurrentRT = gfxOutputBackBuffersColorRTs[currentBackBufferIndex];

		struct TestCB
		{
			Matrix4x4 transform;
			Matrix4x4 view;
			Matrix4x4 viewProjection;
		};

		Gfx::UploadMemoryAllocationInfo testCBAllocationInfo = gfxTransientAllocator.allocate(sizeof(TestCB));
		TestCB* testCB = (TestCB*)testCBAllocationInfo.cpuPointer;
		testCB->transform = Matrix4x4::RotationZ(accum);
		testCB->view = cameraViewMatrix;
		testCB->viewProjection = cameraViewMatrix * cameraProjectionMatrix;

		GfxHAL::CommandList gfxCommandList;
		gfxDevice.openCommandList(gfxCommandList, gfxCommandAllocator);

		gfxCommandList.textureMemoryBarrier(gfxCurrentBackBuffer,
			GfxHAL::BarrierSync::None, GfxHAL::BarrierSync::All,
			GfxHAL::BarrierAccess::None, GfxHAL::BarrierAccess::RenderTarget,
			GfxHAL::TextureLayout::Present, GfxHAL::TextureLayout::RenderTarget);

		const float32x4 clearColor(0.0f, 0.1f, 0.3f, 1.0f);
		gfxCommandList.clearColorRenderTarget(gfxCurrentRT, (float32*)&clearColor);

		gfxCommandList.setRenderTarget(gfxCurrentRT);
		gfxCommandList.setViewport(0.0f, 0.0f, 1600.0f, 900.0f);
		gfxCommandList.setScissor(0, 0, 1600, 900);

		gfxCommandList.setPipelineType(GfxHAL::PipelineType::Graphics);
		gfxCommandList.setPipelineLayout(gfxTestPL);
		gfxCommandList.setPipeline(gfxTestGfxPipeline);

		gfxCommandList.bindIndexBuffer(GfxHAL::BufferPointer { gfxTestBuffer, 4096 }, GfxHAL::IndexBufferFormat::U16, 4096);
		gfxCommandList.bindVertexBuffer(0, GfxHAL::BufferPointer { gfxTestBuffer, 0 }, sizeof(TestVertex), 4096);
		gfxCommandList.bindBuffer("SOME_CONSTANT_BUFFER"_xsh, GfxHAL::BufferBindType::Constant, testCBAllocationInfo.gpuPointer);
		gfxCommandList.drawIndexed(countof(cubeIndices));

		gfxCommandList.textureMemoryBarrier(gfxCurrentBackBuffer,
			GfxHAL::BarrierSync::All, GfxHAL::BarrierSync::None,
			GfxHAL::BarrierAccess::RenderTarget, GfxHAL::BarrierAccess::None,
			GfxHAL::TextureLayout::RenderTarget, GfxHAL::TextureLayout::Present);

		gfxDevice.closeCommandList(gfxCommandList);
		gfxDevice.submitCommandList(GfxHAL::DeviceQueue::Graphics, gfxCommandList);
		gfxDevice.submitOutputFlip(GfxHAL::DeviceQueue::Graphics, gfxOutput);

		const GfxHAL::DeviceQueueSyncPoint gfxEndOfQueueSyncPoint = gfxDevice.getEndOfQueueSyncPoint(GfxHAL::DeviceQueue::Graphics);
		gfxTransientAllocator.enqueueRelease(gfxEndOfQueueSyncPoint);
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
