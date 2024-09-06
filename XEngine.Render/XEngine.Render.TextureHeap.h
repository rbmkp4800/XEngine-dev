#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

namespace XEngine::Render
{
	enum class TextureHandle : uint32 {};

	class TextureHeap : public XLib::NonCopyable
	{
	private:
		struct Entry
		{
			Gfx::HAL::TextureHandle gfxHwTexture;
		};

	private:
		Gfx::HAL::Device* gfxHwDevice = nullptr;
		Entry entries[16] = {};
		uint16 entryCount = 0;

	public:
		TextureHeap() = default;
		~TextureHeap() = default;

		void initialize(Gfx::HAL::Device& gfxHwDevice);
		void destroy();

		TextureHandle loadFromFile(const char* path);
	};

	extern TextureHeap GTextureHeap;
}
