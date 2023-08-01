#include <XLib.h>
#include <XLib.Math.h>

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
	{ { +0.6f, -0.4f, 0.5f }, { 0.0f, 0.0f, -1.0f } },
	{ { -0.6f, -0.4f, 0.5f }, { 0.0f, 0.0f, -1.0f } },
	{ {  0.0f, +0.6f, 0.5f }, { 0.0f, 0.0f, -1.0f } },

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
	GfxHAL::CommandList gfxCommandList;
	GfxHAL::SurfaceHandle gfxSurface = GfxHAL::SurfaceHandle::Zero;
	GfxHAL::TextureHandle gfxSurfaceBuffers[GfxHAL::SurfaceBackBufferCount] = {};
	GfxHAL::RenderTargetViewHandle gfxSurfaceBufferRTVs[GfxHAL::SurfaceBackBufferCount] = {};
	GfxHAL::BufferHandle gfxTestBuffer = GfxHAL::BufferHandle::Zero;
	void* mappedTestBuffer = nullptr;

	Gfx::ShaderLibrary gfxShaderLibrary;
	Gfx::TransientUploadMemoryAllocator gfxTransientAllocator;

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
	gfxTransientAllocator.initialize(gfxDevice, 16);

	gfxSurface = gfxDevice.createWindowSurface(1600, 900, window.getHandle());
	for (int i = 0; i < GfxHAL::SurfaceBackBufferCount; i++)
	{
		gfxSurfaceBuffers[i] = gfxDevice.getSurfaceBackBuffer(gfxSurface, i);
		gfxSurfaceBufferRTVs[i] = gfxDevice.createRenderTargetView(gfxSurfaceBuffers[i], GfxHAL::TexelViewFormat::R8G8B8A8_UNORM);
	}

	gfxTestBuffer = gfxDevice.createBuffer(64 * 1024, false, GfxHAL::BufferMemoryType::Upload);
	mappedTestBuffer = gfxDevice.mapBuffer(gfxTestBuffer);

	TestVertex* vertexBuffer = (TestVertex*)mappedTestBuffer;
	uint16* indexBuffer = (uint16*)(uintptr(mappedTestBuffer) + 4096);

	memoryCopy(vertexBuffer, cubeVertices, sizeof(cubeVertices));
	memoryCopy(indexBuffer, cubeIndices, sizeof(cubeIndices));

	gfxShaderLibrary.load("../Build/XEngine.Render.Shaders.xeslib", gfxDevice);

	const GfxHAL::PipelineLayoutHandle gfxTestPL = gfxShaderLibrary.getPipelineLayout("Test.PipelineLayout"_xsh);
	const GfxHAL::PipelineHandle gfxTestGfxPipeline = gfxShaderLibrary.getPipeline("Test.GraphicsPipeline"_xsh);

	for (;;)
	{
		System::DispatchEvents();

		const uint32 currentBackBufferIndex = gfxDevice.getSurfaceCurrentBackBufferIndex(gfxSurface);
		const GfxHAL::TextureHandle gfxCurrentBackBuffer = gfxSurfaceBuffers[currentBackBufferIndex];
		const GfxHAL::RenderTargetViewHandle gfxCurrentRTV = gfxSurfaceBufferRTVs[currentBackBufferIndex];

		static float qqq = 0.0f;
		qqq += 0.1f;

		struct TestCB
		{
			float32x4 c;
		};

		Gfx::UploadMemoryAllocationInfo testCBAllocationInfo = gfxTransientAllocator.allocate(sizeof(TestCB));
		TestCB* testCB = (TestCB*)testCBAllocationInfo.cpuPointer;
		testCB->c = { 0.0f, 0.0f, Math::Sin(qqq * 10.0f), 1.0f };

		gfxCommandList.open();
		gfxCommandList.textureMemoryBarrier(gfxCurrentBackBuffer,
			GfxHAL::BarrierSync::None, GfxHAL::BarrierSync::All,
			GfxHAL::BarrierAccess::None, GfxHAL::BarrierAccess::RenderTarget,
			GfxHAL::TextureLayout::Present, GfxHAL::TextureLayout::RenderTarget);

		float32 color[4] = { Math::Sin(qqq) * 0.5f + 0.5f, Math::Sin(qqq + 1.0f) * 0.5f + 0.5f, 0.0f, 1.0f };
		gfxCommandList.clearRenderTarget(gfxCurrentRTV, color);

		gfxCommandList.setRenderTarget(gfxCurrentRTV);
		gfxCommandList.setViewport(0.0f, 0.0f, 1600.0f, 900.0f);
		gfxCommandList.setScissor(0, 0, 1600, 900);

		gfxCommandList.setPipelineType(GfxHAL::PipelineType::Graphics);
		gfxCommandList.setPipelineLayout(gfxTestPL);
		gfxCommandList.setPipeline(gfxTestGfxPipeline);

		gfxCommandList.bindIndexBuffer(GfxHAL::BufferPointer { gfxTestBuffer, 4096 }, GfxHAL::IndexBufferFormat::U16, 4096);
		gfxCommandList.bindVertexBuffer(0, GfxHAL::BufferPointer { gfxTestBuffer, 0 }, sizeof(TestVertex), 4096);
		gfxCommandList.bindBuffer("SOME_CONSTANT_BUFFER"_xsh, GfxHAL::BufferBindType::Constant, testCBAllocationInfo.gpuPointer);
		gfxCommandList.drawIndexed(3);

		gfxCommandList.textureMemoryBarrier(gfxCurrentBackBuffer,
			GfxHAL::BarrierSync::All, GfxHAL::BarrierSync::None,
			GfxHAL::BarrierAccess::RenderTarget, GfxHAL::BarrierAccess::None,
			GfxHAL::TextureLayout::RenderTarget, GfxHAL::TextureLayout::Present);

		gfxDevice.submitWorkload(GfxHAL::DeviceQueue::Main, gfxCommandList);
		gfxDevice.submitSurfaceFlip(gfxSurface);

		const GfxHAL::DeviceQueueSyncPoint gfxEndOfQueueSyncPoint = gfxDevice.getEndOfQueueSyncPoint(GfxHAL::DeviceQueue::Main);
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
