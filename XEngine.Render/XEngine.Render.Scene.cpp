#include "XEngine.Render.Scene.h"

using namespace XEngine::Gfx;
using namespace XEngine::Render;

namespace
{
	struct TestVertex
	{
		float32x3 position;
		float32x3 normal;
		float32x2 texcoord;
	};

	const TestVertex CubeVertices[] =
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

	const uint16 CubeIndices[] =
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
}


void Scene::initialize(HAL::Device& gfxHwDevice)
{
	XEAssert(!this->gfxHwDevice);
	this->gfxHwDevice = &gfxHwDevice;

	gfxHwTestModel = gfxHwDevice.createStagingBuffer(64 * 1024, Gfx::HAL::StagingBufferAccessMode::DeviceReadHostWrite);
	mappedTestModel = gfxHwDevice.getMappedBufferPtr(gfxHwTestModel);

	TestVertex* vertexBuffer = (TestVertex*)mappedTestModel;
	uint16* indexBuffer = (uint16*)(uintptr(mappedTestModel) + 4096);

	memoryCopy(vertexBuffer, CubeVertices, sizeof(CubeVertices));
	memoryCopy(indexBuffer, CubeIndices, sizeof(CubeIndices));
}
