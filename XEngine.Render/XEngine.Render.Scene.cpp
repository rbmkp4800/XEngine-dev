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

	{
		XLib::File file;
		file.open("../Content/wood-0.albedo.xerawtex", XLib::FileAccessMode::Read, XLib::FileOpenMode::OpenExisting);
		XEAssert(file.isOpen());

		uint16x2 textureSize ={};

		file.read(&textureSize, sizeof(textureSize));

		void* textureData = XLib::SystemHeapAllocator::Allocate(textureSize.x * textureSize.y * 4);
		file.read(textureData, textureSize.x * textureSize.y * 4);

		const HAL::TextureDesc gfxHwTestTextureDesc =
			HAL::TextureDesc::Create2D(textureSize.x, textureSize.y, HAL::TextureFormat::R8G8B8A8, 1);

		gfxHwTestTexture = gfxHwDevice.createTexture(gfxHwTestTextureDesc, HAL::TextureLayout::Common);

		HAL::TextureRegion gfxHwTextureRegion =
		{
			.offset = uint16x3(0, 0, 0),
			.size = uint16x3(textureSize.x, textureSize.y, 1),
		};

		GlobalUploader.uploadTexture(gfxHwTestTexture, HAL::TextureSubresource {}, gfxHwTextureRegion, textureData, textureSize.x * 4);

		XLib::SystemHeapAllocator::Release(textureData);
	}

	{
		HAL::ResourceView gfxHwTestTextureView = {};
		gfxHwTestTextureView.textureHandle = gfxHwTestTexture;
		gfxHwTestTextureView.texture.format = HAL::TexelViewFormat::R8G8B8A8_UNORM;
		gfxHwTestTextureView.texture.writable = false;
		gfxHwTestTextureView.texture.baseMipLevel = 0;
		gfxHwTestTextureView.texture.mipLevelCount = 1;
		gfxHwTestTextureView.type = HAL::ResourceViewType::Texture;

		gfxHwDevice.writeBindlessDescriptor(0, gfxHwTestTextureView);
	}
}
