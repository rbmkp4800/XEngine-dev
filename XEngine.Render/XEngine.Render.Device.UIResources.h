#pragma once

#include <XLib.NonCopyable.h>
#include <XLib.Platform.COMPtr.h>

struct ID3D12Device;
struct ID3D12RootSignature;
struct ID3D12PipelineState;

namespace XEngine::Render::Device_
{
	class UIResources : public XLib::NonCopyable
	{
	private:
		XLib::Platform::COMPtr<ID3D12RootSignature> d3dDefaultRS;
		XLib::Platform::COMPtr<ID3D12PipelineState> d3dColorPSO;
		XLib::Platform::COMPtr<ID3D12PipelineState> d3dColorAlphaTexturePSO;

	public:
		UIResources() = default;
		~UIResources() = default;

		void initialize(ID3D12Device* d3dDevice);

		inline ID3D12RootSignature* getDefaultRS() { return d3dDefaultRS; }
		inline ID3D12PipelineState* getColorPSO() { return d3dColorPSO; }
		inline ID3D12PipelineState* getColorAlphaTexturePSO() { return d3dColorAlphaTexturePSO; }
	};
}