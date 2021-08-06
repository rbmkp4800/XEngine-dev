#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Vectors.h>
#include <XLib.Platform.COMPtr.h>

struct ID3D12GraphicsCommandList;
struct ID3D12CommandAllocator;

namespace XEngine::Render { class Device; }
namespace XEngine::Render { class Target; }

namespace XEngine::Render
{
	class CustomDrawBatch : public XLib::NonCopyable
	{
		friend Device;

	private:
		Device *device = nullptr;

		XLib::Platform::COMPtr<ID3D12GraphicsCommandList> d3dCommandList;
		XLib::Platform::COMPtr<ID3D12CommandAllocator> d3dCommandAllocator;

	public:
		CustomDrawBatch() = default;
		~CustomDrawBatch() = default;

		void initialize(Device& device);
		void destroy();

		void open();
		void close();

		void drawIndependentMeshlets();

		// drawIndependentMeshlets( meshlet { material, geometry, transform } [] );
		// drawMeshletsArray( meshlets gpu pointer, global constants );
		// draw(vb, material, global constants);
		// drawIndexed();
	};
}
