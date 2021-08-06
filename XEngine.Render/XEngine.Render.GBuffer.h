#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Vectors.h>
#include <XLib.Platform.COMPtr.h>

struct ID3D12Resource;

namespace XEngine::Render { class Device; }
namespace XEngine::Render::Device_ { class Renderer; }

namespace XEngine::Render
{
	class GBuffer : public XLib::NonCopyable
	{
		friend Device_::Renderer;

	private:
		static constexpr uint32 BloomLevelCount = 5;

		struct SRVDescriptorIndex
		{
			enum
			{
				Diffuse = 0,
				NormalRoughnessMetalness,
				Depth,
				HDR,
				BloomA,
				BloomB,
				DownscaledX2Depth,

				Count,
			};
		};

		struct RTVDescriptorIndex
		{
			enum
			{
				Diffuse = 0,
				NormalRoughnessMetalness,
				HDR,
				BloomABase,
				BloomBBase = BloomABase + BloomLevelCount,
				DownscaledX2Depth = BloomBBase + BloomLevelCount,
				DownscaledX4Depth,

				Count,
			};
		};

	private:
		Device *device = nullptr;

		XLib::Platform::COMPtr<ID3D12Resource> d3dDiffuseTexture;
		XLib::Platform::COMPtr<ID3D12Resource> d3dNormalRoughnessMetalnessTexture;
		XLib::Platform::COMPtr<ID3D12Resource> d3dDepthTexture;
		XLib::Platform::COMPtr<ID3D12Resource> d3dHDRTexture;
		XLib::Platform::COMPtr<ID3D12Resource> d3dBloomTextureA;
		XLib::Platform::COMPtr<ID3D12Resource> d3dBloomTextureB;
		XLib::Platform::COMPtr<ID3D12Resource> d3dDownscaledX2DepthTexture;
		XLib::Platform::COMPtr<ID3D12Resource> d3dDownscaledX4DepthTexture;

		uint16x2 size = { 0, 0 };

		uint16 srvDescriptorsBaseIndex = 0;
		uint16 rtvDescriptorsBaseIndex = 0;
		uint16 dsvDescriptorIndex = 0;

	public:
		GBuffer() = default;
		~GBuffer() = default;

		void initialize(Device& device, uint16x2 size);
		void destroy();

		void resize(uint16x2 size);

		inline bool isInitialized() { return device != nullptr; }
	};
}