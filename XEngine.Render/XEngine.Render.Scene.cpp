#include <XLib.Allocation.h>
#include <XLib.System.File.h>
#include <XEngine.Gfx.Uploader.h>

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
		{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f} },
		{ {  1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f} },
		{ {  1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f} },
		{ { -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f} },

		{ { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f} },
		{ {  1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f} },
		{ {  1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f} },
		{ { -1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f} },

		{ { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f} },
		{ {  1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f} },
		{ { -1.0f,  1.0f,  1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f} },
		{ {  1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f} },

		{ { -1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f} },
		{ {  1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f} },
		{ {  1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f} },
		{ { -1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f} },

		{ { -1.0f, -1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f} },
		{ { -1.0f,  1.0f,  1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f} },
		{ { -1.0f, -1.0f,  1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f} },
		{ { -1.0f,  1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f} },

		{ {  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f} },
		{ {  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f} },
		{ {  1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f} },
		{ {  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f} },
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


HAL::TextureHandle Scene::LoadTexture(HAL::Device& gfxHwDevice, const char* path)
{
	XLib::File file;
	file.open(path, XLib::FileAccessMode::Read, XLib::FileOpenMode::OpenExisting);
	XEAssert(file.isOpen());

	uint16x2 textureSize = {};

	file.read(&textureSize, sizeof(textureSize));
	XEAssert(textureSize.x > 0 && textureSize.x <= 8192);
	XEAssert(textureSize.y > 0 && textureSize.y <= 8192);

	void* textureData = XLib::SystemHeapAllocator::Allocate(textureSize.x * textureSize.y * 4);
	file.read(textureData, textureSize.x * textureSize.y * 4);

	const HAL::TextureHandle gfxHwTexture = gfxHwDevice.createTexture(
		HAL::TextureDesc::Create2D(textureSize.x, textureSize.y, HAL::TextureFormat::R8G8B8A8, 1));

	const HAL::TextureRegion gfxHwTextureRegion =
	{
		.offset = uint16x3(0, 0, 0),
		.size = uint16x3(textureSize.x, textureSize.y, 1),
	};
	GlobalUploader.uploadTexture(gfxHwTexture, HAL::TextureSubresource {}, gfxHwTextureRegion, textureData, textureSize.x * 4);

	XLib::SystemHeapAllocator::Release(textureData);

	return gfxHwTexture;
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

	gfxHwTestAlbedoTexture = LoadTexture(gfxHwDevice, "../Content/wood-0.albedo.xerawtex");
	gfxHwTestNRMTexture = LoadTexture(gfxHwDevice, "../Content/wood-0.nrm.xerawtex");

	gfxHwDevice.writeBindlessDescriptor(0, HAL::ResourceView::CreateTexture2D(gfxHwTestAlbedoTexture, HAL::TexelViewFormat::R8G8B8A8_UNORM));
	gfxHwDevice.writeBindlessDescriptor(1, HAL::ResourceView::CreateTexture2D(gfxHwTestNRMTexture, HAL::TexelViewFormat::R8G8B8A8_UNORM));
}
