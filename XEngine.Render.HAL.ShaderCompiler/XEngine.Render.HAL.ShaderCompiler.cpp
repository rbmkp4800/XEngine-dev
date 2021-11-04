#include <d3d12.h>
//#include <d3d12shader.h>
#include <dxcapi.h>

#include <XLib.Containers.ArrayList.h>
#include <XLib.Platform.COMPtr.h>

#include <XEngine.Render.HAL.ObjectFormat.h>

#include "XEngine.Render.HAL.ShaderCompiler.h"

using namespace XLib;
using namespace XLib::Platform;
using namespace XEngine::Render::HAL;
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
	if (desc.bindPointCount >= MaxPipelineBindPointCount)
		return false;

	D3D12_ROOT_PARAMETER1 d3dRootParams[MaxPipelineBindPointCount] = {};
	BindPointId bindPointIds[MaxPipelineBindPointCount] = {};

	uint8 cbvRegisterCount = 0;
	uint8 srvRegisterCount = 0;
	uint8 uavRegisterCount = 0;

	for (uint32 i = 0; i < desc.bindPointCount; i++)
	{
		const PipelineBindPointDesc& bindPoint = desc.bindPoints[i];

		D3D12_ROOT_PARAMETER1& d3dRootParam = d3dRootParams[i];

		if  (bindPoint.type == PipelineBindPointType::Constants)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			d3dRootParam.Constants.ShaderRegister = cbvRegisterCount;
			d3dRootParam.Constants.RegisterSpace = 0;
			d3dRootParam.Constants.Num32BitValues = bindPoint.constantsSize32bitValues;

			cbvRegisterCount++;
		}
		else if (bindPoint.type == PipelineBindPointType::ConstantBuffer)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			d3dRootParam.Descriptor.ShaderRegister = cbvRegisterCount;
			d3dRootParam.Descriptor.RegisterSpace = 0;
			d3dRootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE; // TODO: Data volatility

			cbvRegisterCount++;
		}
		else
			return false;

		d3dRootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // TODO: ...
	}

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3dRootSignatureDesc = {};
	d3dRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	d3dRootSignatureDesc.Desc_1_1.pParameters = d3dRootParams;
	d3dRootSignatureDesc.Desc_1_1.NumParameters = desc.bindPointCount;
	d3dRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	// TODO: D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
	// TODO: D3D12_ROOT_SIGNATURE_FLAG_DENY_*_SHADER_ROOT_ACCESS

	COMPtr<ID3DBlob> d3dRootSignature, d3dError;
	D3D12SerializeVersionedRootSignature(&d3dRootSignatureDesc, d3dRootSignature.initRef(), d3dError.initRef());

	const uint32 headerOffset = 0;
	const uint32 bindPointIdsOffset = headerOffset + sizeof(ObjectFormat::PipelineLayoutObjectHeader);
	const uint32 rootSignatureOffset = bindPointIdsOffset + sizeof(BindPointId) * desc.bindPointCount;

	const uint32 objectSize = rootSignatureOffset + d3dRootSignature->GetBufferSize();

	result.objectData.release();
	result.objectData.allocate(objectSize);
	byte* objectDataBytes = (byte*)result.objectData.getMutablePointer();

	ObjectFormat::PipelineLayoutObjectHeader header = {};
	header.signature = ObjectFormat::PipelineLayoutObjectSignature;
	header.version = ObjectFormat::PipelineLayoutObjectCurrentVerstion;
	header.objectSize = objectSize;
	header.objectHash = 0; // TODO: ...
	header.sourceHash = 0; // TODO: ...
	header.bindPointCount = desc.bindPointCount;

	Memory::Copy(objectDataBytes + headerOffset, &header, sizeof(ObjectFormat::PipelineLayoutObjectHeader));
	Memory::Copy(objectDataBytes + bindPointIdsOffset, bindPointIds, sizeof(BindPointId) * desc.bindPointCount);
	Memory::Copy(objectDataBytes + rootSignatureOffset, d3dRootSignature->GetBufferPointer(), d3dRootSignature->GetBufferSize());

	return true;
}

bool Host::CompileShader(Platform platform, const CompiledPipelineLayout& compiledPipelineLayout,
	ShaderType shaderType, const char* source, uint32 sourceLength, CompiledShader& result)
{
	COMPtr<IDxcCompiler3> dxcCompiler;
	DxcCreateInstance(CLSID_DxcCompiler, dxcCompiler.uuid(), dxcCompiler.voidInitRef());

	DxcBuffer dxcSourceBuffer = {};
	dxcSourceBuffer.Ptr = source;
	dxcSourceBuffer.Size = sourceLength;
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
