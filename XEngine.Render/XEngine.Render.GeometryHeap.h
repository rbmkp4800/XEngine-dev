#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

namespace XEngine::Render { class SceneRenderer; }

namespace XEngine::Render
{
	enum class GeometryHandle : uint32 {};

	class GeometryHeap : public XLib::NonCopyable
	{
		friend SceneRenderer;

	private:
		struct Entry
		{
			uint32 vertexBufferOffset;
			uint32 indexBufferOffset;
			uint32 vertexCount;
			uint32 indexCount;
			uint16 vertexStride;
		};

	private:
		Gfx::HAL::Device* gfxHwDevice = nullptr;

		Gfx::HAL::BufferHandle gfxHwGeometryPool = {};
		uintptr mappedGeometryPool = 0;
		uint32 geometryPoolAllocatedSize = 0;

		Entry entries[16] = {};
		uint16 entryCount = 0;

	public:
		GeometryHeap() = default;
		~GeometryHeap() = default;

		void initialize(Gfx::HAL::Device& gfxHwDevice);
		void destroy();

		GeometryHandle createTestCube();
	};

	extern GeometryHeap GGeometryHeap;
}
