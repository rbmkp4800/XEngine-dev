#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>
#include <XLib.Color.h>

#include "XEngine.Render.Base.h"
#include "XEngine.Render.Device.BufferHeap.h"
#include "XEngine.Render.Device.GeometryHeap.h"
#include "XEngine.Render.Device.GPUDescriptorHeap.h"
#include "XEngine.Render.Device.MaterialHeap.h"
#include "XEngine.Render.Device.Renderer.h"
#include "XEngine.Render.Device.ShadersLoader.h"
#include "XEngine.Render.Device.TextureHeap.h"
#include "XEngine.Render.Device.UIResources.h"
#include "XEngine.Render.Device.Uploader.h"

struct IDXGIFactory5;
struct ID3D12Device2;
struct ID3D12CommandQueue;
struct ID3D12Fence;

namespace XEngine::Render { struct Camera; }
namespace XEngine::Render { class Scene; }
namespace XEngine::Render { class Target; }
namespace XEngine::Render { class GBuffer; }
namespace XEngine::Render { class Output; }
namespace XEngine::Render { class CustomDrawBatch; }

namespace XEngine::Render
{
	class Device : public XLib::NonCopyable
	{
		friend Device_::BufferHeap;
		friend Device_::TextureHeap;
		friend Device_::MaterialHeap;
		friend Device_::GeometryHeap;

		friend Device_::Renderer;
		friend Device_::Uploader;

		friend Scene;
		friend GBuffer;
		friend Output;
		friend CustomDrawBatch;

	private:
		static XLib::Platform::COMPtr<IDXGIFactory5> dxgiFactory;

		XLib::Platform::COMPtr<ID3D12Device2> d3dDevice;

		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dGraphicsQueue;
		XLib::Platform::COMPtr<ID3D12CommandQueue> d3dCopyQueue;

		XLib::Platform::COMPtr<ID3D12Fence> d3dCopyQueueFence;

		uint64 copyQueueFenceValue = 0;

		Device_::GPUDescriptorHeap srvHeap;
		Device_::GPUDescriptorHeap rtvHeap;
		Device_::GPUDescriptorHeap dsvHeap;

		Device_::BufferHeap bufferHeap;
		Device_::TextureHeap textureHeap;
		Device_::MaterialHeap materialHeap;
		Device_::GeometryHeap geometryHeap;

		Device_::ShadersLoader shadersLoader;
		Device_::Renderer renderer;
		Device_::Uploader uploader; // TODO: maybe rename to AsyncUploader

		Device_::UIResources uiResources;

		uint64 graphicsQueueClockFrequency = 0;

	private:
		void deviceThreadRoutine();
		static void DeviceThreadRoutine(void* self);

	public:
		using QueueSyncPoint = uint64;
		using SceneRenderId = uint32;

	public:
		Device();
		~Device() = default;

		bool initialize();
		void destroy();

		inline BufferAddress allocateBuffer(uint32 size);
		inline void releaseBuffer(BufferAddress address);

		inline TextureHandle createTexture(uint16 width, uint16 height);
		inline void destroyTexture(TextureHandle handle);

		//inline MaterialShaderHandle createMaterialShader(...);
		inline MaterialShaderHandle createDefaultMaterialShader();
		inline void destroyMaterialShader(MaterialShaderHandle handle);

		inline MaterialInstanceHandle createMaterialInstance(MaterialShaderHandle baseShader);
		inline void destroyMaterialInstance(MaterialInstanceHandle handle);

		inline GeometrySectionBundleHandle createGeometrySectionBundle(
			const GeometrySectionDesc* sections, uint32 sectionCount/*, uint32 materialSetSize*/);
		inline void destroyGeometrySectionBundle(GeometrySectionBundleHandle handle);

		inline void uploadBufferBlocking(BufferAddress destinationAddress, const void* sourceData, uint32 size);
		inline void uploadTextureBlocking(TextureHandle texture, const void* sourceData, uint32 sourceRowPitch);

		//void uploadBufferAsync(BufferAddress destinationAddress, const void* sourceData, uint32 size); // , async stuff);
		//void uploadTextureAsync(TextureHandle texture, const void* sourceData, uint32 sourceRowPitch); // , async stuff);

		void updateMaterialInstanceConstants(MaterialInstanceHandle instance, uint32 offset, const void* data, uint32 size);
		void updateMaterialInstanceTexture(MaterialInstanceHandle instance, uint32 slot, TextureHandle textureHandle);
		//void updateGeometrySectionBundleMaterialSet(GeometrySectionBundleHandle bundle,
		//	uint8 startIndex, uint8 count, const MaterialInstanceHandle* materials);

		SceneRenderId graphicsQueueSceneRender(Scene& scene, const Camera& camera, GBuffer& gBuffer, uint16x2 viewport, Target& target);
		void graphicsQueueCustomDraw(CustomDrawBatch& batch);
		void graphicsQueueOutputFlip(Output& output);

		QueueSyncPoint getEndOfGraphicsQueueSyncPoint();
		bool isQueueSyncPointReached(QueueSyncPoint syncPoint);

		void resolveSceneRenderTimings(SceneRenderId id);
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XEngine::Render
{
	inline BufferAddress Device::allocateBuffer(uint32 size) { return bufferHeap.allocate(size); }
	inline void Device::releaseBuffer(BufferAddress address) { bufferHeap.release(address); }

	inline MaterialShaderHandle Device::createDefaultMaterialShader() { return materialHeap.createDefaultMaterialShader(); }

	inline MaterialInstanceHandle Device::createMaterialInstance(MaterialShaderHandle baseShader)
	{
		return materialHeap.createMaterialInstance(baseShader);
	}

	inline GeometrySectionBundleHandle Device::createGeometrySectionBundle(const GeometrySectionDesc* sections, uint32 sectionCount)
	{
		return geometryHeap.createSectionBundle(sections, sectionCount);
	}

	inline void Device::uploadBufferBlocking(BufferAddress destinationAddress, const void* sourceData, uint32 size)
	{
		uploader.uploadBuffer(bufferHeap.getArenaD3DResource(), uint32(destinationAddress), sourceData, size);
	}
}
