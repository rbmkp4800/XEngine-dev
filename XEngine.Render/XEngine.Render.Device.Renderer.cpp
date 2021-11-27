// TODO: "Downsample" depth, not "downscale"!
// TODO: Inverse barriers logic. SRV by default, transition to RT before rendering.
// TODO: Move PS data to the beginning of Root Signature

#include <d3d12.h>

#include <XLib.Math.Matrix4x4.h>
#include <XLib.Platform.D3D12.Helpers.h>

#include "XEngine.Render.Device.Renderer.h"

#include "XEngine.Render.Device.h"
#include "XEngine.Render.Camera.h"
#include "XEngine.Render.Scene.h"
#include "XEngine.Render.Target.h"
#include "XEngine.Render.GBuffer.h"
#include "XEngine.Render.Internal.Shaders.h"

using namespace XLib;
using namespace XLib::Platform;
using namespace XEngine::Render;
using namespace XEngine::Render::Internal;
using namespace XEngine::Render::Device_;

namespace
{
	struct CameraTransformConstants
	{
		Matrix4x4 view;
		Matrix4x4 viewProjection;
	};

	struct LightingPassConstants
	{
		Matrix4x4 view;
		Matrix4x4 projection;
		float32x2 depthDeprojectionCoefs;
		float32 aspect;
		float32 halfFovTan;
	};
}

void Renderer::initialize()
{
	ID3D12Device* d3dDevice = device.d3dDevice;

	d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		d3dDefaultCA.uuid(), d3dDefaultCA.voidInitRef());
	d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
		d3dDefaultCA, nullptr, d3dDefaultCL.uuid(), d3dDefaultCL.voidInitRef());
	d3dDefaultCL->Close();

	// G-buffer pass RS
	{
		D3D12_DESCRIPTOR_RANGE ranges[] =
		{
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 1, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
		};

		D3D12_ROOT_PARAMETER rootParameters[] =
		{
				// Global b0: camera transform constant buffer
			D3D12RootParameter_CBV(0, 0, D3D12_SHADER_VISIBILITY_ALL),

				// AS t0: global buffer
			D3D12RootParameter_SRV(0, 0, D3D12_SHADER_VISIBILITY_AMPLIFICATION),
				// AS t1: transforms buffer
			D3D12RootParameter_SRV(1, 0, D3D12_SHADER_VISIBILITY_AMPLIFICATION),
				// AS t2: section records buffer
			D3D12RootParameter_SRV(2, 0, D3D12_SHADER_VISIBILITY_AMPLIFICATION),

				// MS t0: global buffer
			D3D12RootParameter_SRV(0, 0, D3D12_SHADER_VISIBILITY_MESH),
				// MS t1: transforms buffer
			D3D12RootParameter_SRV(1, 0, D3D12_SHADER_VISIBILITY_MESH),

				// PS b1 materials table
			//D3D12RootParameter_CBV(1, 0, D3D12_SHADER_VISIBILITY_PIXEL),
				// PS t0 space1 textures
			//D3D12RootParameter_Table(countof(ranges), ranges, D3D12_SHADER_VISIBILITY_PIXEL),
		};

		D3D12_STATIC_SAMPLER_DESC staticSamplers[] =
		{
			D3D12StaticSamplerDesc_Default(0, 0, D3D12_SHADER_VISIBILITY_PIXEL),
		};

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = countof(rootParameters);
		desc.pParameters = rootParameters;
		desc.NumStaticSamplers = countof(staticSamplers);
		desc.pStaticSamplers= staticSamplers;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		COMPtr<ID3DBlob> d3dSignature, d3dError;
		D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0,
			d3dSignature.initRef(), d3dError.initRef());

		d3dDevice->CreateRootSignature(0,
			d3dSignature->GetBufferPointer(), d3dSignature->GetBufferSize(),
			d3dGBufferPassRS.uuid(), d3dGBufferPassRS.voidInitRef());
	}

	// Lighting pass RS
	{
		D3D12_DESCRIPTOR_RANGE gBufferRanges[] =
		{
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
		};
		D3D12_DESCRIPTOR_RANGE shadowMapRanges[] =
		{
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
		};

		D3D12_ROOT_PARAMETER rootParameters[] =
		{
				// b0 lighting pass constants
			D3D12RootParameter_CBV(0, 0, D3D12_SHADER_VISIBILITY_PIXEL),
				// t0-t3 G-buffer textures
			D3D12RootParameter_Table(countof(gBufferRanges), gBufferRanges, D3D12_SHADER_VISIBILITY_PIXEL),
		};

		D3D12_STATIC_SAMPLER_DESC staticSamplers[] =
		{
				// s0 shadow sampler
			D3D12StaticSamplerDesc(0, 0, D3D12_SHADER_VISIBILITY_PIXEL,
				D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_COMPARISON_FUNC_LESS),
		};

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = countof(rootParameters);
		desc.pParameters = rootParameters;
		desc.NumStaticSamplers = countof(staticSamplers);
		desc.pStaticSamplers = staticSamplers;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		COMPtr<ID3DBlob> d3dSignature, d3dError;
		D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0,
			d3dSignature.initRef(), d3dError.initRef());

		d3dDevice->CreateRootSignature(0,
			d3dSignature->GetBufferPointer(), d3dSignature->GetBufferSize(),
			d3dLightingPassRS.uuid(), d3dLightingPassRS.voidInitRef());
	}

	// Post-process RS
	{
		D3D12_DESCRIPTOR_RANGE ranges[] =
		{
			{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
		};

		D3D12_ROOT_PARAMETER rootParameters[] =
		{
			D3D12RootParameter_Constants(3, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL),
			D3D12RootParameter_Table(countof(ranges), ranges, D3D12_SHADER_VISIBILITY_PIXEL),
		};

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		staticSampler.MipLODBias = 0.0f;
		staticSampler.MaxAnisotropy = 1;
		staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		staticSampler.MinLOD = 0.0f;
		staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
		staticSampler.ShaderRegister = 0;
		staticSampler.RegisterSpace = 0;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = countof(rootParameters);
		desc.pParameters = rootParameters;
		desc.NumStaticSamplers = 1;
		desc.pStaticSamplers = &staticSampler;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		COMPtr<ID3DBlob> d3dSignature, d3dError;
		D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0,
			d3dSignature.initRef(), d3dError.initRef());

		d3dDevice->CreateRootSignature(0,
			d3dSignature->GetBufferPointer(), d3dSignature->GetBufferSize(),
			d3dPostProcessRS.uuid(), d3dPostProcessRS.voidInitRef());
	}

	// Lighting pass PSO
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = d3dLightingPassRS;
		psoDesc.VS = D3D12ShaderBytecode(Shaders::ScreenQuadVS.data, Shaders::ScreenQuadVS.size);
		psoDesc.PS = D3D12ShaderBytecode(Shaders::LightingPassPS.data, Shaders::LightingPassPS.size);
		psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = D3D12RasterizerDesc_Default();
		psoDesc.DepthStencilState = D3D12DepthStencilDesc_Disable();
		psoDesc.InputLayout = D3D12InputLayoutDesc();
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R11G11B10_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		d3dDevice->CreateGraphicsPipelineState(&psoDesc,
			d3dLightingPassPSO.uuid(), d3dLightingPassPSO.voidInitRef());
	}

	// Tone mapping PSO
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = d3dPostProcessRS;
		psoDesc.VS = D3D12ShaderBytecode(Shaders::ScreenQuadVS.data, Shaders::ScreenQuadVS.size);
		psoDesc.PS = D3D12ShaderBytecode(Shaders::ToneMappingPS.data, Shaders::ToneMappingPS.size);
		psoDesc.BlendState = D3D12BlendDesc_NoBlend();
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = D3D12RasterizerDesc_Default();
		psoDesc.DepthStencilState = D3D12DepthStencilDesc_Disable();
		psoDesc.InputLayout = D3D12InputLayoutDesc();
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		d3dDevice->CreateGraphicsPipelineState(&psoDesc,
			d3dToneMappingPSO.uuid(), d3dToneMappingPSO.voidInitRef());
	}

	// Upload buffer
	{
		d3dDevice->CreateCommittedResource(
			&D3D12HeapProperties(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
			&D3D12ResourceDesc_Buffer(0x1'0000),
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			d3dUploadBuffer.uuid(), d3dUploadBuffer.voidInitRef());

		d3dUploadBuffer->Map(0, &D3D12Range(), (void**)&mappedUploadBuffer);
	}

	// Readback buffer
	{
		d3dDevice->CreateCommittedResource(&D3D12HeapProperties(D3D12_HEAP_TYPE_READBACK), D3D12_HEAP_FLAG_NONE,
			&D3D12ResourceDesc_Buffer(0x1'0000), D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
			d3dReadbackBuffer.uuid(), d3dReadbackBuffer.voidInitRef());
	}

	d3dDevice->CreateQueryHeap(&D3D12QueryHeapDesc(D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 16),
		d3dTimestampQueryHeap.uuid(), d3dTimestampQueryHeap.voidInitRef());

	d3dDevice->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, d3dFence.uuid(), d3dFence.voidInitRef());
}

void Renderer::render(Scene& scene, const Camera& camera, GBuffer& gBuffer, Target& target, rectu16 viewport)
{
	//device.d3dGraphicsQueue->GetClockCalibration(&gpuTimerCalibrationTimespamp, &cpuTimerCalibrationTimespamp);

	const uint16x2 viewportSize = viewport.getSize();
	const float32x2 viewportSizeF = float32x2(viewportSize);

	const float32 invCameraClipPlanesDelta = 1.0f / (camera.zFar - camera.zNear);
	const float32x2 depthDeprojectionCoefs(
		(camera.zFar * camera.zNear) * invCameraClipPlanesDelta,
		camera.zFar * invCameraClipPlanesDelta);

	CameraTransformConstants cameraTransformConstants = {};
	LightingPassConstants lightingPassConstants = {};

	// Updating frame constants
	{
		const float32 aspect = viewportSizeF.x / viewportSizeF.y;
		const Matrix4x4 view = camera.getViewMatrix();
		const Matrix4x4 projection = camera.getProjectionMatrix(aspect);
		const Matrix4x4 viewProjection = view * projection;
		cameraTransformConstants.view = view;
		cameraTransformConstants.viewProjection = viewProjection;

		lightingPassConstants.view = view;
		lightingPassConstants.projection = projection;
		lightingPassConstants.depthDeprojectionCoefs = depthDeprojectionCoefs;
		lightingPassConstants.aspect = aspect;
		lightingPassConstants.halfFovTan = Math::Tan(camera.fov * 0.5f);
	}

	const uint32 cameraTransformConstantsOffset = 0;
	const uint32 lightingPassConstantsOffset = alignUp<uint32>(
		cameraTransformConstantsOffset + sizeof(LightingPassConstants),
		D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	memoryCopy(mappedUploadBuffer + cameraTransformConstantsOffset, &cameraTransformConstants, sizeof(cameraTransformConstants));
	memoryCopy(mappedUploadBuffer + lightingPassConstantsOffset, &lightingPassConstants, sizeof(lightingPassConstants));

	const D3D12_GPU_VIRTUAL_ADDRESS uploadBufferAddress = d3dUploadBuffer->GetGPUVirtualAddress();

	{
		d3dDefaultCA->Reset();
		d3dDefaultCL->Reset(d3dDefaultCA, nullptr);

		ID3D12DescriptorHeap* d3dDescriptorHeaps[] = { device.srvHeap.getD3D12DescriptorHeap() };
		d3dDefaultCL->SetDescriptorHeaps(countof(d3dDescriptorHeaps), d3dDescriptorHeaps);
	}

	// G-buffer pass
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorsHandle = device.rtvHeap.getCPUHandle(
			gBuffer.rtvDescriptorsBaseIndex + GBuffer::RTVDescriptorIndex::Diffuse);
		D3D12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle = device.dsvHeap.getCPUHandle(gBuffer.dsvDescriptorIndex);
		d3dDefaultCL->OMSetRenderTargets(3, &rtvDescriptorsHandle, TRUE, &dsvDescriptorHandle);

		D3D12_CPU_DESCRIPTOR_HANDLE hdrRTVDescriptorsHandle = device.rtvHeap.getCPUHandle(
			gBuffer.rtvDescriptorsBaseIndex + GBuffer::RTVDescriptorIndex::HDR);

		const float32 clearColor[4] = {};
		d3dDefaultCL->ClearRenderTargetView(rtvDescriptorsHandle, clearColor, 0, nullptr);
		d3dDefaultCL->ClearRenderTargetView(hdrRTVDescriptorsHandle, clearColor, 0, nullptr);
		d3dDefaultCL->ClearDepthStencilView(dsvDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		d3dDefaultCL->RSSetViewports(1, &D3D12ViewPort(0.0f, 0.0f, viewportSizeF.x, viewportSizeF.y));
		d3dDefaultCL->RSSetScissorRects(1, &D3D12Rect(0, 0, viewportSize.x, viewportSize.y));

		d3dDefaultCL->SetGraphicsRootSignature(d3dGBufferPassRS);

		// global b0: camera transform constants
		d3dDefaultCL->SetGraphicsRootConstantBufferView(0, uploadBufferAddress + cameraTransformConstantsOffset);
		// AS t0: global buffer
		d3dDefaultCL->SetGraphicsRootShaderResourceView(1, device.bufferHeap.getArenaD3DResource()->GetGPUVirtualAddress()); 
		// AS t1: transforms buffer
		d3dDefaultCL->SetGraphicsRootShaderResourceView(2, scene.d3dTransformsBuffer->GetGPUVirtualAddress());
		// MS t0: global buffer
		d3dDefaultCL->SetGraphicsRootShaderResourceView(4, device.bufferHeap.getArenaD3DResource()->GetGPUVirtualAddress());
		// MS t1: transforms buffer
		d3dDefaultCL->SetGraphicsRootShaderResourceView(5, scene.d3dTransformsBuffer->GetGPUVirtualAddress());

		for (Scene::DrawBatchDesc& batch : scene.drawBatches)
		{
			// AS t2: section records buffer
			d3dDefaultCL->SetGraphicsRootShaderResourceView(3, scene.d3dDrawArgsBuffer->GetGPUVirtualAddress() + batch.sectionsRecordsBaseOffsetX256 * 256);

			d3dDefaultCL->SetPipelineState(device.materialHeap.getDefaultMaterialPSO());

			d3dDefaultCL->DispatchMesh(batch.sectionCount, 1, 1);
		}
	}

	// Lighting pass
	{
		{
			D3D12_RESOURCE_BARRIER d3dBarriers[] =
			{
				D3D12ResourceBarrier_Transition(gBuffer.d3dDiffuseTexture,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				D3D12ResourceBarrier_Transition(gBuffer.d3dNormalRoughnessMetalnessTexture,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				D3D12ResourceBarrier_Transition(gBuffer.d3dDepthTexture,
					D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			};

			d3dDefaultCL->ResourceBarrier(countof(d3dBarriers), d3dBarriers);
		}

		D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle = device.rtvHeap.getCPUHandle(
			gBuffer.rtvDescriptorsBaseIndex + GBuffer::RTVDescriptorIndex::HDR);
		d3dDefaultCL->OMSetRenderTargets(1, &rtvDescriptorHandle, FALSE, nullptr);

		d3dDefaultCL->RSSetViewports(1, &D3D12ViewPort(0.0f, 0.0f, viewportSizeF.x, viewportSizeF.y));
		d3dDefaultCL->RSSetScissorRects(1, &D3D12Rect(0, 0, viewportSize.x, viewportSize.y));

		d3dDefaultCL->SetGraphicsRootSignature(d3dLightingPassRS);
		d3dDefaultCL->SetGraphicsRootConstantBufferView(0, uploadBufferAddress + lightingPassConstantsOffset);
		d3dDefaultCL->SetGraphicsRootDescriptorTable(1, device.srvHeap.getGPUHandle(gBuffer.srvDescriptorsBaseIndex));

		d3dDefaultCL->SetPipelineState(d3dLightingPassPSO);

		d3dDefaultCL->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		d3dDefaultCL->DrawInstanced(3, 1, 0, 0);

		{
			D3D12_RESOURCE_BARRIER d3dBarriers[] =
			{
				D3D12ResourceBarrier_Transition(gBuffer.d3dDiffuseTexture,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
				D3D12ResourceBarrier_Transition(gBuffer.d3dNormalRoughnessMetalnessTexture,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
				D3D12ResourceBarrier_Transition(gBuffer.d3dDepthTexture,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE),
			};

			d3dDefaultCL->ResourceBarrier(countof(d3dBarriers), d3dBarriers);
		}
	}

	// Tone map
	{
		{
			D3D12_RESOURCE_BARRIER d3dBarriers[] =
			{
				D3D12ResourceBarrier_Transition(gBuffer.d3dHDRTexture,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				D3D12ResourceBarrier_Transition(target.d3dTexture,
					D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET),
			};

			d3dDefaultCL->ResourceBarrier(countof(d3dBarriers), d3dBarriers);
		}

		d3dDefaultCL->SetGraphicsRootSignature(d3dPostProcessRS);

		d3dDefaultCL->SetPipelineState(d3dToneMappingPSO);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle =
			device.rtvHeap.getCPUHandle(target.rtvDescriptorIndex);
		d3dDefaultCL->OMSetRenderTargets(1, &rtvDescriptorHandle, FALSE, nullptr);

		d3dDefaultCL->RSSetViewports(1, &D3D12ViewPort(0.0f, 0.0f, viewportSizeF.x, viewportSizeF.y));
		d3dDefaultCL->RSSetScissorRects(1, &D3D12Rect(0, 0, viewportSize.x, viewportSize.y));

		d3dDefaultCL->SetGraphicsRoot32BitConstant(0, as<UINT>(1.0f), 0);
		// NOTE: assuming that HDR texture and BloomA texture descriptors are single range
		d3dDefaultCL->SetGraphicsRootDescriptorTable(1, device.srvHeap.getGPUHandle(
			gBuffer.srvDescriptorsBaseIndex + GBuffer::SRVDescriptorIndex::HDR));

		d3dDefaultCL->DrawInstanced(3, 1, 0, 0);

		{
			D3D12_RESOURCE_BARRIER d3dBarriers[] =
			{
				D3D12ResourceBarrier_Transition(gBuffer.d3dHDRTexture,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
				D3D12ResourceBarrier_Transition(target.d3dTexture,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON),
			};

			d3dDefaultCL->ResourceBarrier(countof(d3dBarriers), d3dBarriers);
		}

		d3dDefaultCL->SetGraphicsRootSignature(d3dPostProcessRS);
	}

	{
		d3dDefaultCL->Close();

		ID3D12CommandList *d3dCommandListsToExecute[] = { d3dDefaultCL };
		device.d3dGraphicsQueue->ExecuteCommandLists(
			countof(d3dCommandListsToExecute), d3dCommandListsToExecute);

		fenceValue++;
		device.d3dGraphicsQueue->Signal(d3dFence, fenceValue);
		if (d3dFence->GetCompletedValue() < fenceValue)
			d3dFence->SetEventOnCompletion(fenceValue, nullptr);
	}
}
