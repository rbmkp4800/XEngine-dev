#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>

#include "XEngine.Render.Base.h"

struct ID3D12Resource;

namespace XEngine::Render { class Device; }

namespace XEngine::Render::Device_
{
	class BufferHeap : public XLib::NonCopyable
	{
	private:
		static constexpr uint32 BufferAlignment = 256;

	private:
		Device& device;

		XLib::Platform::COMPtr<ID3D12Resource> d3dArenaBuffer;
		uint32 allocatedByteCount = 0;

	public:
		BufferHeap(Device& device) : device(device) {}
		~BufferHeap() = default;

		void initialize();

		BufferAddress allocate(uint32 size);
		void release(BufferAddress ptr);

		ID3D12Resource* getArenaD3DResource() { return d3dArenaBuffer; }
	};
}
