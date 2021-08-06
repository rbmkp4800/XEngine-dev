#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Vectors.h>
#include <XLib.Platform.COMPtr.h>

struct ID3D12Resource;
struct IDXGISwapChain3;

namespace XEngine::Render { class Device; }
namespace XEngine::Render { class Output; }
namespace XEngine::Render { class CustomDrawBatch; }
namespace XEngine::Render::Device_ { class Renderer; }

namespace XEngine::Render
{
	class Target : public XLib::NonCopyable
	{
		friend Output;
		friend CustomDrawBatch;
		friend Device_::Renderer;

	private:
		XLib::Platform::COMPtr<ID3D12Resource> d3dTexture;
		uint16 rtvDescriptorIndex = 0;
		bool stateRenderTarget = false;
		
	public:
		Target() = default;
		~Target() = default;
	};

	class Output : public XLib::NonCopyable
	{
		friend Device;

	private:
		static constexpr uint32 bufferCount = 2;

	private:
		Device *device = nullptr;
		XLib::Platform::COMPtr<IDXGISwapChain3> dxgiSwapChain;
		Target buffers[bufferCount];
		uint16x2 size;

	private:
		void updateRTVs(bool allocateDescriptors);

	public:
		Output() = default;
		~Output() = default;

		void initialize(Device& device, void* hWnd, uint16x2 size);
		void destroy();

		void resize(uint16x2 size);

		Target& getCurrentTarget();

		inline uint16x2 getSize() const { return size; }
	};
}
