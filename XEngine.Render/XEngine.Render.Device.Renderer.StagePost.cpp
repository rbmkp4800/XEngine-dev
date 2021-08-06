#include "XEngine.Render.Device.Renderer.StagePost.h"

using namespace XEngine::Render::Device_::Renderer_;

void StagePost::initialize()
{
	// Bloom PSOs
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = d3dPostProcessRS;
		psoDesc.VS = D3D12ShaderBytecode(Shaders::ScreenQuadVS.data, Shaders::ScreenQuadVS.size);
		//     .PS customized
		psoDesc.BlendState = D3D12BlendDesc_NoBlend();
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = D3D12RasterizerDesc_Default();
		psoDesc.DepthStencilState = D3D12DepthStencilDesc_Disable();
		psoDesc.InputLayout = D3D12InputLayoutDesc();
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R11G11B10_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		psoDesc.PS = D3D12ShaderBytecode(Shaders::BloomFilterAndDownscalePS.data, Shaders::BloomFilterAndDownscalePS.size);
		d3dDevice->CreateGraphicsPipelineState(&psoDesc, d3dBloomFilterAndDownscalePSO.uuid(), d3dBloomFilterAndDownscalePSO.voidInitRef());

		psoDesc.PS = D3D12ShaderBytecode(Shaders::BloomDownscalePS.data, Shaders::BloomDownscalePS.size);
		d3dDevice->CreateGraphicsPipelineState(&psoDesc, d3dBloomDownscalePSO.uuid(), d3dBloomDownscalePSO.voidInitRef());

		psoDesc.PS = D3D12ShaderBytecode(Shaders::BloomBlurHorizontalPS.data, Shaders::BloomBlurHorizontalPS.size);
		d3dDevice->CreateGraphicsPipelineState(&psoDesc, d3dBloomBlurHorizontalPSO.uuid(), d3dBloomBlurHorizontalPSO.voidInitRef());

		psoDesc.PS = D3D12ShaderBytecode(Shaders::BloomBlurVerticalPS.data, Shaders::BloomBlurVerticalPS.size);
		d3dDevice->CreateGraphicsPipelineState(&psoDesc, d3dBloomBlurVerticalPSO.uuid(), d3dBloomBlurVerticalPSO.voidInitRef());

		psoDesc.PS = D3D12ShaderBytecode(Shaders::BloomBlurVerticalAndAccumulatePS.data, Shaders::BloomBlurVerticalAndAccumulatePS.size);
		d3dDevice->CreateGraphicsPipelineState(&psoDesc, d3dBloomBlurVerticalAndAccumulatePSO.uuid(), d3dBloomBlurVerticalAndAccumulatePSO.voidInitRef());
	}
}

void StagePost::execute()
{
	{
		{
			D3D12_RESOURCE_BARRIER d3dBarriers[] =
			{
				D3D12ResourceBarrier_Transition(gBuffer.d3dHDRTexture,
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			{},
			};

			uint32 barrierCount = countof(d3dBarriers);
			if (target.stateRenderTarget)
				barrierCount--;
			else
			{
				target.stateRenderTarget = true;
				d3dBarriers[1] = D3D12ResourceBarrier_Transition(target.d3dTexture,
					D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_RENDER_TARGET);
			}

			d3dFrameFinishCL->ResourceBarrier(barrierCount, d3dBarriers);
		}

		d3dFrameFinishCL->SetGraphicsRootSignature(d3dPostProcessRS);

		// Generate bloom ===================================================================//

		uint16x2 bloomLevelDims[GBuffer::BloomLevelCount];
		{
			const uint16 alignment = 1 << GBuffer::BloomLevelCount;
			bloomLevelDims[0].x = alignup(viewportSize.x / 4, alignment);
			bloomLevelDims[0].y = alignup(viewportSize.y / 4, alignment);

			for (uint32 i = 1; i < GBuffer::BloomLevelCount; i++)
				bloomLevelDims[i] = bloomLevelDims[i - 1] / 2;
		}

		// Filter and downscale HDR buffer to first bloom level

		{
			d3dFrameFinishCL->SetPipelineState(d3dBloomFilterAndDownscalePSO);

			D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle = device.rtvHeap.getCPUHandle(
				gBuffer.rtvDescriptorsBaseIndex + GBuffer::RTVDescriptorIndex::BloomABase);
			d3dFrameFinishCL->OMSetRenderTargets(1, &rtvDescriptorHandle, FALSE, nullptr);

			const uint16x2 dim = bloomLevelDims[0];
			d3dFrameFinishCL->RSSetViewports(1, &D3D12ViewPort(0.0f, 0.0f, dim.x, dim.y));
			d3dFrameFinishCL->RSSetScissorRects(1, &D3D12Rect(0, 0, dim.x, dim.y));

			float32x2 sampleOffset = float32x2(0.25f, 0.25f) / float32x2(bloomLevelDims[0]);
			d3dFrameFinishCL->SetGraphicsRoot32BitConstants(0, 2, &sampleOffset, 0);
			d3dFrameFinishCL->SetGraphicsRootDescriptorTable(1, device.srvHeap.getGPUHandle(
				gBuffer.srvDescriptorsBaseIndex + GBuffer::SRVDescriptorIndex::HDR));

			d3dFrameFinishCL->DrawInstanced(3, 1, 0, 0);
		}

		// Downscale filtered bloom

		d3dFrameFinishCL->ResourceBarrier(1,
			&D3D12ResourceBarrier_Transition(gBuffer.d3dBloomTextureA,
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 0));

		d3dFrameFinishCL->SetPipelineState(d3dBloomDownscalePSO);

		d3dFrameFinishCL->SetGraphicsRootDescriptorTable(1, device.srvHeap.getGPUHandle(
			gBuffer.srvDescriptorsBaseIndex + GBuffer::SRVDescriptorIndex::BloomA));

		for (uint32 bloomLevel = 1; bloomLevel < GBuffer::BloomLevelCount; bloomLevel++)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle = device.rtvHeap.getCPUHandle(
				gBuffer.rtvDescriptorsBaseIndex + GBuffer::RTVDescriptorIndex::BloomABase + bloomLevel);
			d3dFrameFinishCL->OMSetRenderTargets(1, &rtvDescriptorHandle, FALSE, nullptr);

			const uint16x2 dim = bloomLevelDims[bloomLevel];
			d3dFrameFinishCL->RSSetViewports(1, &D3D12ViewPort(0.0f, 0.0f, dim.x, dim.y));
			d3dFrameFinishCL->RSSetScissorRects(1, &D3D12Rect(0, 0, dim.x, dim.y));

			d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(float32(bloomLevel - 1)), 0);
			d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(0.5f / float32(dim.x)), 1);
			d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(0.5f / float32(dim.y)), 2);

			d3dFrameFinishCL->DrawInstanced(3, 1, 0, 0);

			d3dFrameFinishCL->ResourceBarrier(1,
				&D3D12ResourceBarrier_Transition(gBuffer.d3dBloomTextureA,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, bloomLevel));
		}

		// Blur bloom levels horizontally

		d3dFrameFinishCL->SetPipelineState(d3dBloomBlurHorizontalPSO);

		for (uint32 bloomLevel = 0; bloomLevel < GBuffer::BloomLevelCount; bloomLevel++)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle = device.rtvHeap.getCPUHandle(
				gBuffer.rtvDescriptorsBaseIndex + GBuffer::RTVDescriptorIndex::BloomBBase + bloomLevel);
			d3dFrameFinishCL->OMSetRenderTargets(1, &rtvDescriptorHandle, FALSE, nullptr);

			const uint16x2 dim = bloomLevelDims[bloomLevel];
			d3dFrameFinishCL->RSSetViewports(1, &D3D12ViewPort(0.0f, 0.0f, dim.x, dim.y));
			d3dFrameFinishCL->RSSetScissorRects(1, &D3D12Rect(0, 0, dim.x, dim.y));

			d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(float32(bloomLevel)), 0);
			d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(1.0f / float32(dim.x)), 1);
			d3dFrameFinishCL->SetGraphicsRootDescriptorTable(1, device.srvHeap.getGPUHandle(
				gBuffer.srvDescriptorsBaseIndex + GBuffer::SRVDescriptorIndex::BloomA));

			d3dFrameFinishCL->DrawInstanced(3, 1, 0, 0);
		}

		// Blur last bloom level vertically

		{
			d3dFrameFinishCL->SetPipelineState(d3dBloomBlurVerticalPSO);

			D3D12_RESOURCE_BARRIER d3dBarriers[] =
			{
				D3D12ResourceBarrier_Transition(gBuffer.d3dBloomTextureA,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET,
				GBuffer::BloomLevelCount - 1),
				D3D12ResourceBarrier_Transition(gBuffer.d3dBloomTextureB,
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				GBuffer::BloomLevelCount - 1),
			};
			d3dFrameFinishCL->ResourceBarrier(countof(d3dBarriers), d3dBarriers);

			D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle = device.rtvHeap.getCPUHandle(
				gBuffer.rtvDescriptorsBaseIndex + GBuffer::RTVDescriptorIndex::BloomABase + GBuffer::BloomLevelCount - 1);
			d3dFrameFinishCL->OMSetRenderTargets(1, &rtvDescriptorHandle, FALSE, nullptr);

			const uint16x2 dim = bloomLevelDims[GBuffer::BloomLevelCount - 1];
			d3dFrameFinishCL->RSSetViewports(1, &D3D12ViewPort(0.0f, 0.0f, dim.x, dim.y));
			d3dFrameFinishCL->RSSetScissorRects(1, &D3D12Rect(0, 0, dim.x, dim.y));

			d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(float32(GBuffer::BloomLevelCount - 1)), 0);
			d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(1.0f / float32(dim.y)), 1);
			d3dFrameFinishCL->SetGraphicsRootDescriptorTable(1, device.srvHeap.getGPUHandle(
				gBuffer.srvDescriptorsBaseIndex + GBuffer::SRVDescriptorIndex::BloomB));

			d3dFrameFinishCL->DrawInstanced(3, 1, 0, 0);

			d3dFrameFinishCL->ResourceBarrier(1,
				&D3D12ResourceBarrier_Transition(gBuffer.d3dBloomTextureA,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					GBuffer::BloomLevelCount - 1));
		}

		// Blur previous bloom levels vertically and accumulate result

		d3dFrameFinishCL->SetPipelineState(d3dBloomBlurVerticalAndAccumulatePSO);

		d3dFrameFinishCL->SetGraphicsRootDescriptorTable(1, device.srvHeap.getGPUHandle(
			gBuffer.srvDescriptorsBaseIndex + GBuffer::SRVDescriptorIndex::BloomA));

		for (sint32 bloomLevel = GBuffer::BloomLevelCount - 2; bloomLevel >= 0; bloomLevel--)
		{
			D3D12_RESOURCE_BARRIER d3dBarriers[] =
			{
				D3D12ResourceBarrier_Transition(gBuffer.d3dBloomTextureA,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET, bloomLevel),
				D3D12ResourceBarrier_Transition(gBuffer.d3dBloomTextureB,
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, bloomLevel),
			};
			d3dFrameFinishCL->ResourceBarrier(countof(d3dBarriers), d3dBarriers);

			D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle = device.rtvHeap.getCPUHandle(
				gBuffer.rtvDescriptorsBaseIndex + GBuffer::RTVDescriptorIndex::BloomABase + bloomLevel);
			d3dFrameFinishCL->OMSetRenderTargets(1, &rtvDescriptorHandle, FALSE, nullptr);

			const uint16x2 dim = bloomLevelDims[bloomLevel];
			d3dFrameFinishCL->RSSetViewports(1, &D3D12ViewPort(0.0f, 0.0f, dim.x, dim.y));
			d3dFrameFinishCL->RSSetScissorRects(1, &D3D12Rect(0, 0, dim.x, dim.y));

			d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(float32(bloomLevel)), 0);
			d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(1.0f / float32(dim.y)), 1);
			d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(float32(1.0f)), 2);

			d3dFrameFinishCL->DrawInstanced(3, 1, 0, 0);

			d3dFrameFinishCL->ResourceBarrier(1,
				&D3D12ResourceBarrier_Transition(gBuffer.d3dBloomTextureA,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, bloomLevel));
		}

		// Tone map =========================================================================//

		d3dFrameFinishCL->SetPipelineState(d3dToneMappingPSO);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle =
			device.rtvHeap.getCPUHandle(target.rtvDescriptorIndex);
		d3dFrameFinishCL->OMSetRenderTargets(1, &rtvDescriptorHandle, FALSE, nullptr);

		d3dFrameFinishCL->RSSetViewports(1, &D3D12ViewPort(0.0f, 0.0f, viewportSizeF.x, viewportSizeF.y));
		d3dFrameFinishCL->RSSetScissorRects(1, &D3D12Rect(0, 0, viewportSize.x, viewportSize.y));

		d3dFrameFinishCL->SetGraphicsRoot32BitConstant(0, as<UINT>(1.0f), 0);
		// NOTE: assuming that HDR texture and BloomA texture descriptors are single range
		d3dFrameFinishCL->SetGraphicsRootDescriptorTable(1, device.srvHeap.getGPUHandle(
			gBuffer.srvDescriptorsBaseIndex + GBuffer::SRVDescriptorIndex::HDR));

		d3dFrameFinishCL->DrawInstanced(3, 1, 0, 0);

		{
			D3D12_RESOURCE_BARRIER d3dBarriers[2] =
			{
				D3D12ResourceBarrier_Transition(gBuffer.d3dBloomTextureA,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
				D3D12ResourceBarrier_Transition(gBuffer.d3dBloomTextureB,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
			};
			// NOTE: this can be done as a split barrier
			d3dFrameFinishCL->ResourceBarrier(countof(d3dBarriers), d3dBarriers);
		}

		// Debug wireframe ==================================================================//
		// TODO: move from here
		if (debugOutput == DebugOutput::Wireframe)
		{
			d3dFrameFinishCL->SetGraphicsRootSignature(d3dGBufferPassRS);
			// NOTE: assuming that first matrix in this CB is view VP matrix
			d3dFrameFinishCL->SetGraphicsRootConstantBufferView(4, cameraTransformConstantsAddress);
			d3dFrameFinishCL->SetPipelineState(d3dDebugWireframePSO);

			scene.populateCommandListForGBufferPass(d3dFrameFinishCL, d3dGBufferPassICS, false);
		}

		{
			D3D12_RESOURCE_BARRIER d3dBarriers[] =
			{
				D3D12ResourceBarrier_Transition(gBuffer.d3dHDRTexture,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
			{},
			};

			uint32 barrierCount = countof(d3dBarriers);
			if (finalizeTarget)
			{
				target.stateRenderTarget = false;
				d3dBarriers[1] = D3D12ResourceBarrier_Transition(target.d3dTexture,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
			}
			else
				barrierCount--;

			d3dFrameFinishCL->ResourceBarrier(barrierCount, d3dBarriers);
		}
	}
}
