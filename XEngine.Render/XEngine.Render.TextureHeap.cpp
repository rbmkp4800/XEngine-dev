#include <XLib.Allocation.h>
#include <XLib.System.File.h>
#include <XEngine.Gfx.Uploader.h>

#include "XEngine.Render.TextureHeap.h"

using namespace XEngine::Gfx;
using namespace XEngine::Render;

TextureHeap XEngine::Render::GTextureHeap;

void TextureHeap::initialize(HAL::Device& gfxHwDevice)
{
	XEAssert(!this->gfxHwDevice);
	this->gfxHwDevice = &gfxHwDevice;
}

TextureHandle TextureHeap::loadFromFile(const char* path)
{
	XEAssert(entryCount < countOf(entries));
	const uint32 entryIndex = entryCount;
	entryCount++;

	XLib::File file;
	file.open(path, XLib::FileAccessMode::Read, XLib::FileOpenMode::OpenExisting);
	XEAssert(file.isOpen());

	uint16x2 textureSize = {};

	file.read(&textureSize, sizeof(textureSize));
	XEAssert(textureSize.x > 0 && textureSize.x <= 8192);
	XEAssert(textureSize.y > 0 && textureSize.y <= 8192);

	void* textureData = XLib::SystemHeapAllocator::Allocate(textureSize.x * textureSize.y * 4);
	file.read(textureData, textureSize.x * textureSize.y * 4);
	file.close();

	const HAL::TextureHandle gfxHwTexture = gfxHwDevice->createTexture(
		HAL::TextureDesc::Create2D(textureSize.x, textureSize.y, HAL::TextureFormat::R8G8B8A8, 1));

	const HAL::TextureRegion gfxHwTextureRegion =
	{
		.offset = uint16x3(0, 0, 0),
		.size = uint16x3(textureSize.x, textureSize.y, 1),
	};
	GUploader.uploadTexture(gfxHwTexture, HAL::TextureSubresource {}, gfxHwTextureRegion, textureData, textureSize.x * 4);
	XLib::SystemHeapAllocator::Release(textureData);

	gfxHwDevice->writeBindlessDescriptor(entryIndex, HAL::ResourceView::CreateTexture2D(gfxHwTexture, HAL::TexelViewFormat::R8G8B8A8_UNORM));

	Entry& entry = entries[entryIndex];
	entry.gfxHwTexture = gfxHwTexture;

	return TextureHandle(entryIndex);
}
