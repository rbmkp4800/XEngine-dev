#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>

#include "XEngine.Render.Base.h"

struct ID3D12RootSignature;
struct ID3D12CommandSignature;
struct ID3D12PipelineState;
struct ID3D12GraphicsCommandList6;
struct ID3D12CommandAllocator;
struct ID3D12Resource;
struct ID3D12QueryHeap;
struct ID3D12Fence;

namespace XEngine::Render { struct Camera; }
namespace XEngine::Render { class Device; }
namespace XEngine::Render { class Scene; }
namespace XEngine::Render { class Target; }
namespace XEngine::Render { class GBuffer; }

namespace XEngine::Render::Device_
{
	class Renderer : public XLib::NonCopyable
	{
	private:
		Device& device;

		XLib::Platform::COMPtr<ID3D12CommandAllocator> d3dDefaultCA;
		XLib::Platform::COMPtr<ID3D12GraphicsCommandList6> d3dDefaultCL;

		XLib::Platform::COMPtr<ID3D12RootSignature> d3dGBufferPassRS;
		XLib::Platform::COMPtr<ID3D12RootSignature> d3dLightingPassRS;
		XLib::Platform::COMPtr<ID3D12RootSignature> d3dPostProcessRS;

		XLib::Platform::COMPtr<ID3D12PipelineState> d3dLightingPassPSO;
		XLib::Platform::COMPtr<ID3D12PipelineState> d3dToneMappingPSO;

		XLib::Platform::COMPtr<ID3D12Resource> d3dUploadBuffer;
		XLib::Platform::COMPtr<ID3D12Resource> d3dReadbackBuffer;

		byte* mappedUploadBuffer = nullptr;

		XLib::Platform::COMPtr<ID3D12Fence> d3dFence;
		uint64 fenceValue = 0;

		XLib::Platform::COMPtr<ID3D12QueryHeap> d3dTimestampQueryHeap;

		//uint64 cpuTimerCalibrationTimespamp = 0, gpuTimerCalibrationTimespamp = 0;

		// Renderer_::StageGBuffer stageGBuffer;
		// Renderer_::StageLighting stageLighting;
		// Renderer_::StagePost stagePost;

	public:
		Renderer(Device& device) : device(device) {}
		~Renderer() = default;

		void initialize();
		void destroy();

		void render(Scene& scene, const Camera& camera, GBuffer& gBuffer, Target& target, rectu16 viewport);

		inline ID3D12RootSignature* getGBufferPassD3DRS() { return d3dGBufferPassRS; }
	};
}
