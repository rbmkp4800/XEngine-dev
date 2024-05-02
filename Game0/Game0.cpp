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

	Gfx::HAL::Device gfxDevice;
	Gfx::HAL::CommandAllocatorHandle gfxCommandAllocator;

	Gfx::HAL::OutputHandle gfxOutput = Gfx::HAL::OutputHandle::Zero;
	Gfx::HAL::TextureHandle gfxDepthTexture = Gfx::HAL::TextureHandle::Zero;

	Gfx::HAL::BufferHandle gfxTestBuffer = Gfx::HAL::BufferHandle::Zero;
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

	Gfx::HAL::DeviceSettings gfxDeviceSettings = {};

	gfxDeviceSettings.maxCommandAllocatorCount = 4;
	gfxDeviceSettings.maxDescriptorAllocatorCount = 4;
	gfxDeviceSettings.maxCommandListCount = 4;
	gfxDeviceSettings.maxMemoryAllocationCount = 1024;
	gfxDeviceSettings.maxResourceCount = 1024;
	gfxDeviceSettings.maxDescriptorSetLayoutCount = 64;
	gfxDeviceSettings.maxPipelineLayoutCount = 64;
	gfxDeviceSettings.maxShaderCount = 64;
	gfxDeviceSettings.maxCompositePipelineCount = 64;
	gfxDeviceSettings.maxOutputCount = 4;
	gfxDeviceSettings.bindlessDescriptorPoolSize = 16;

	gfxDevice.initialize(gfxDeviceSettings);
	gfxCommandAllocator = gfxDevice.createCommandAllocator();
	gfxTransientAllocator.initialize(gfxDevice, 16);

	gfxOutput = gfxDevice.createWindowOutput(1600, 900, window.getHandle());

	Gfx::HAL::TextureDesc gfxDepthTextureDesc = {};
	gfxDepthTextureDesc.size = { 1600, 900, 1 };
	gfxDepthTextureDesc.dimension = Gfx::HAL::TextureDimension::Texture2D;
	gfxDepthTextureDesc.format = Gfx::HAL::TextureFormat::D32;
	gfxDepthTextureDesc.mipLevelCount = 1;
	gfxDepthTextureDesc.enableRenderTargetUsage = true;

	gfxDepthTexture = gfxDevice.createTexture(gfxDepthTextureDesc, Gfx::HAL::TextureLayout::DepthStencilRenderTarget);

	gfxTestBuffer = gfxDevice.createStagingBuffer(64 * 1024, Gfx::HAL::StagingBufferAccessMode::DeviceReadHostWrite);
	mappedTestBuffer = gfxDevice.getMappedBufferPtr(gfxTestBuffer);

	TestVertex* vertexBuffer = (TestVertex*)mappedTestBuffer;
	uint16* indexBuffer = (uint16*)(uintptr(mappedTestBuffer) + 4096);

	memoryCopy(vertexBuffer, cubeVertices, sizeof(cubeVertices));
	memoryCopy(indexBuffer, cubeIndices, sizeof(cubeIndices));

	gfxShaderLibrary.load("XEngine.Render.Shaders.xeslib", gfxDevice);

	const Gfx::HAL::PipelineLayoutHandle gfxTestPL = gfxShaderLibrary.getPipelineLayout("Test.PipelineLayout"_xsh);
	const Gfx::HAL::ShaderHandle gfxTestVS = gfxShaderLibrary.getShader("TestVS"_xsh);
	const Gfx::HAL::ShaderHandle gfxTestPS = gfxShaderLibrary.getShader("TestPS"_xsh);

	Gfx::HAL::VertexAttribute attributes[] =
	{
		{ "POSITION",	0,	Gfx::HAL::TexelViewFormat::R32G32B32_FLOAT,	},
		{ "NORMAL",		12,	Gfx::HAL::TexelViewFormat::R32G32B32_FLOAT,	},
		{ "TEXCOORD",	24,	Gfx::HAL::TexelViewFormat::R32G32_FLOAT,	},
	};

	Gfx::HAL::GraphicsPipelineDesc gfxTestGfxPipelineDesc = {};
	gfxTestGfxPipelineDesc.vertexAttributes = attributes;
	gfxTestGfxPipelineDesc.vertexAttributeCount = countof(attributes);
	gfxTestGfxPipelineDesc.vsHandle = gfxTestVS;
	gfxTestGfxPipelineDesc.psHandle = gfxTestPS;
	gfxTestGfxPipelineDesc.colorRenderTargetFormats[0] = Gfx::HAL::TexelViewFormat::R8G8B8A8_UNORM;
	gfxTestGfxPipelineDesc.depthStencilRenderTargetFormat = Gfx::HAL::DepthStencilFormat::D32;
	gfxTestGfxPipelineDesc.depthStencilState.depthReadEnable = true;
	gfxTestGfxPipelineDesc.depthStencilState.depthWriteEnable = true;
	gfxTestGfxPipelineDesc.depthStencilState.depthComparisonFunc = Gfx::HAL::ComparisonFunc::Less;

	const Gfx::HAL::GraphicsPipelineHandle gfxTestGfxPipeline = gfxDevice.createGraphicsPipeline(gfxTestPL, gfxTestGfxPipelineDesc);

	cameraPosition = { -13.0f, -7.0f, 10.0f };
	cameraRotation = { 0.0f, -0.7f };

	for (;;)
	{
		System::DispatchEvents();

		accum += 0.005f;

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
		const Gfx::HAL::TextureHandle gfxCurrentBackBuffer = gfxDevice.getOutputBackBuffer(gfxOutput, currentBackBufferIndex);

		Gfx::HAL::CommandList gfxCommandList;
		gfxDevice.openCommandList(gfxCommandList, gfxCommandAllocator);

		gfxCommandList.textureMemoryBarrier(gfxCurrentBackBuffer,
			Gfx::HAL::BarrierSync::None, Gfx::HAL::BarrierSync::All,
			Gfx::HAL::BarrierAccess::None, Gfx::HAL::BarrierAccess::ColorRenderTarget,
			Gfx::HAL::TextureLayout::Present, Gfx::HAL::TextureLayout::ColorRenderTarget);

		Gfx::HAL::ColorRenderTarget colorRT =
		{
			.texture = gfxCurrentBackBuffer,
			.format = Gfx::HAL::TexelViewFormat::R8G8B8A8_UNORM,
		};

		Gfx::HAL::DepthStencilRenderTarget dsRT = { .texture = gfxDepthTexture, };

		gfxCommandList.bindRenderTargets(1, &colorRT, &dsRT);
		gfxCommandList.setViewport(0.0f, 0.0f, 1600.0f, 900.0f);
		gfxCommandList.setScissor(0, 0, 1600, 900);

		gfxCommandList.setPipelineType(Gfx::HAL::PipelineType::Graphics);
		gfxCommandList.setPipelineLayout(gfxTestPL);
		gfxCommandList.setGraphicsPipeline(gfxTestGfxPipeline);

		gfxCommandList.bindIndexBuffer(Gfx::HAL::BufferPointer { gfxTestBuffer, 4096 }, Gfx::HAL::IndexBufferFormat::U16, 4096);
		gfxCommandList.bindVertexBuffer(0, Gfx::HAL::BufferPointer { gfxTestBuffer, 0 }, sizeof(TestVertex), 4096);
		
		const float32x4 clearColor(0.0f, 0.1f, 0.3f, 1.0f);
		gfxCommandList.clearColorRenderTarget(0, (float32*)&clearColor);
		gfxCommandList.clearDepthStencilRenderTarget(true, false, 1.0f, 0);

		for (int i = 0; i < 27; i++)
		{
			struct TestCB
			{
				Matrix4x4 transform;
				Matrix4x4 view;
				Matrix4x4 viewProjection;
			};

			int r = i % 3;
			int c = i % 9 / 3;
			int s = i / 9;

			float32x3 o =
			{
				float32(r - 1) * 3.0f,
				float32(c - 1) * 3.0f,
				float32(s - 1) * 3.0f,
			};

			Gfx::UploadMemoryAllocationInfo testCBAllocationInfo = gfxTransientAllocator.allocate(sizeof(TestCB));
			TestCB* testCB = (TestCB*)testCBAllocationInfo.cpuPointer;
			testCB->transform = Matrix4x4::Translation(o) * Matrix4x4::RotationZ(accum);
			testCB->view = cameraViewMatrix;
			testCB->viewProjection = cameraViewMatrix * cameraProjectionMatrix;

			gfxCommandList.bindBuffer("SOME_CONSTANT_BUFFER"_xsh, Gfx::HAL::BufferBindType::Constant, testCBAllocationInfo.gpuPointer);
			gfxCommandList.drawIndexed(countof(cubeIndices));
		}

		gfxCommandList.textureMemoryBarrier(gfxCurrentBackBuffer,
			Gfx::HAL::BarrierSync::All, Gfx::HAL::BarrierSync::None,
			Gfx::HAL::BarrierAccess::ColorRenderTarget, Gfx::HAL::BarrierAccess::None,
			Gfx::HAL::TextureLayout::ColorRenderTarget, Gfx::HAL::TextureLayout::Present);

		gfxDevice.closeCommandList(gfxCommandList);
		gfxDevice.submitCommandList(Gfx::HAL::DeviceQueue::Graphics, gfxCommandList);
		gfxDevice.submitOutputFlip(Gfx::HAL::DeviceQueue::Graphics, gfxOutput);

		const Gfx::HAL::DeviceQueueSyncPoint gfxEndOfQueueSyncPoint = gfxDevice.getEndOfQueueSyncPoint(Gfx::HAL::DeviceQueue::Graphics);
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
