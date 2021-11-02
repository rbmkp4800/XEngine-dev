#include <d3d12.h>
//#include <d3d12shader.h>
#include <dxcapi.h>

#include <XLib.Containers.ArrayList.h>
#include <XLib.Platform.COMPtr.h>

#include "XEngine.Render.HAL.ShaderCompiler.h"

using namespace XLib;
using namespace XLib::Platform;
using namespace XEngine::Render::HAL::ShaderCompiler;

namespace
{
	struct DXCIncludeHandler : public IDxcIncludeHandler
	{
		virtual HRESULT __stdcall LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override;

		virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override { return E_NOINTERFACE; }
		virtual ULONG __stdcall AddRef() override { return 1; }
		virtual ULONG __stdcall Release() override { return 1; }
	};
}

HRESULT __stdcall ::DXCIncludeHandler::LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource)
{
	return S_FALSE;
}

bool Host::CompilePipelineLayout(Platform platform, const PipelineLayoutDesc& desc, CompiledPipelineLayout& result)
{
	InplaceArrayList<D3D12_ROOT_PARAMETER1, 32> d3dRootParamsList;

	for (uint32 i = 0; i < desc.bindPointCount; i++)
	{
		const PipelineBindPointDesc& bindPoint = desc.bindPoints[i];

		D3D12_ROOT_PARAMETER1 d3dRootParam = {};
		// ...
		d3dRootParamsList.pushBack(d3dRootParam);
	}

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3dRootSignatureDesc = {};
	d3dRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	d3dRootSignatureDesc.Desc_1_1.pParameters = d3dRootParamsList.getData();
	d3dRootSignatureDesc.Desc_1_1.NumParameters = d3dRootParamsList.getSize();
	d3dRootSignatureDesc.Desc_1_1.Flags = ;// D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

	COMPtr<ID3DBlob> d3dRootSignature, d3dError;
	D3D12SerializeVersionedRootSignature(&d3dRootSignatureDesc, d3dRootSignature.initRef(), d3dError.initRef());
}

bool Host::CompileShader(Platform platform, const CompiledPipelineLayout& compiledPipelineLayout,
	ShaderType shaderType, const char* source, uint32 sourceLength, CompiledShader& result)
{
	COMPtr<IDxcCompiler3> dxcCompiler;
	DxcCreateInstance(CLSID_DxcCompiler, dxcCompiler.uuid(), dxcCompiler.voidInitRef());

	DxcBuffer dxcSourceBuffer = {};
	dxcSourceBuffer.Ptr = ...;
	dxcSourceBuffer.Size = ...;
	dxcSourceBuffer.Encoding = CP_UTF8;

	using DXCArgsList = ExpandableInplaceArrayList<LPCWSTR, 32, uint16, false>;
	DXCArgsList dxcArgsList;

	// Build arguments list
	{
		dxcArgsList.pushBack(L"-enable-16bit-types");

		LPCWSTR dxcProfile = nullptr;
		switch (shaderType)
		{
			case ShaderType::Compute:		dxcProfile = L"-Tcs_6_6"; break;
			case ShaderType::Vertex:		dxcProfile = L"-Tvs_6_6"; break;
			case ShaderType::Amplification: dxcProfile = L"-Tas_6_6"; break;
			case ShaderType::Mesh:			dxcProfile = L"-Tms_6_6"; break;
			case ShaderType::Pixel:			dxcProfile = L"-Tps_6_6"; break;
		}
		// ASSERT(dxcTargetProfile);
		dxcArgsList.pushBack(dxcProfile);
	}

	DXCIncludeHandler dxcIncludeHandler;

	COMPtr<IDxcResult> dxcResult;

	dxcCompiler->Compile(&dxcSourceBuffer, dxcArgsList.getData(), dxcArgsList.getSize(),
		&dxcIncludeHandler, dxcResult.uuid(), dxcResult.voidInitRef());
}

bool Host::CompileGraphicsPipeline(Platform platform, const CompiledPipelineLayout& compiledPipelineLayout,
	const GraphicsPipelineDesc& pipelineDesc, CompiledPipeline& result)
{

}
