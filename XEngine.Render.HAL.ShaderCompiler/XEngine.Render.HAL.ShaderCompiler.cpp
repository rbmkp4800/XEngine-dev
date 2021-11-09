#include <d3d12.h>
//#include <d3d12shader.h>
#include <dxcapi.h>

#include <XLib.Containers.ArrayList.h>
#include <XLib.CRC.h>
#include <XLib.Platform.COMPtr.h>
#include <XLib.String.h>
#include <XLib.SystemHeapAllocator.h>
#include <XLib.System.Threading.Atomics.h>

#include <XEngine.Render.HAL.ObjectFormat.h>

#include "XEngine.Render.HAL.ShaderCompiler.h"

using namespace XLib;
using namespace XLib::Platform;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::HAL::ShaderCompiler;
using namespace XEngine::Render::HAL::ShaderCompiler::Internal;

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

struct SharedDataBufferRef::BlockHeader
{
	AtomicU32 referenceCount;
	uint32 dataSize;
	uint64 _padding;
};

void* SharedDataBufferRef::getMutablePointer()
{
	// XEAssert(!block);
	return block + 1;
}

SharedDataBufferRef SharedDataBufferRef::createReference()
{
	// XEAssert(block);
	block->referenceCount.increment();
	SharedDataBufferRef newReference;
	newReference.block = block;
	return newReference;
}

void SharedDataBufferRef::release()
{
	if (!block)
		return;

	const uint32 newReferenceCount = block->referenceCount.decrement();
	if (newReferenceCount == 0)
		SystemHeapAllocator::Release(block);
	block = nullptr;
}

SharedDataBufferRef SharedDataBufferRef::AllocateBuffer(uint32 size)
{
	// XEAssert(size);
	SharedDataBufferRef reference;
	reference.block = (BlockHeader*)SystemHeapAllocator::Allocate(sizeof(BlockHeader) + size);
	reference.block->referenceCount = 1;
	reference.block->dataSize = size;
	reference.block->_padding = 0;
	return reference;
}

bool Host::CompilePipelineLayout(Platform platform, const PipelineLayoutDesc& desc, CompiledPipelineLayout& result)
{
	if (desc.bindPointCount >= MaxPipelineBindPointCount)
		return false;

	D3D12_ROOT_PARAMETER1 d3dRootParams[MaxPipelineBindPointCount] = {};
	ObjectFormat::PipelineBindPointRecord bindPointRecords[MaxPipelineBindPointCount] = {};

	uint8 cbvRegisterCount = 0;
	uint8 srvRegisterCount = 0;
	uint8 uavRegisterCount = 0;

	uint8 rootParameterCount = 0;
	for (uint32 i = 0; i < desc.bindPointCount; i++)
	{
		const PipelineBindPointDesc& bindPointDesc = desc.bindPoints[i];
		ObjectFormat::PipelineBindPointRecord& objectBindPointRecord = bindPointRecords[i];

		const uint8 rootParameterIndex = rootParameterCount;
		rootParameterCount++;

		// XEAssert(objectBindPointRecord.nameCRC);
		const uintptr bindPointNameLength = ComputeCStrLength(bindPointDesc.name);
		const uint32 bindPointNameCRC = CRC32::Compute(bindPointDesc.name, bindPointNameLength);

		objectBindPointRecord.nameCRC = bindPointNameCRC;
		objectBindPointRecord.type = bindPointDesc.type;
		objectBindPointRecord.rootParameterIndex = rootParameterIndex;
		if (bindPointDesc.type == PipelineBindPointType::Constants)
			objectBindPointRecord.constantsSize32bitValues = bindPointDesc.constantsSize32bitValues;

		D3D12_ROOT_PARAMETER1& d3dRootParam = d3dRootParams[rootParameterIndex];

		if (bindPointDesc.type == PipelineBindPointType::Constants)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			d3dRootParam.Constants.ShaderRegister = cbvRegisterCount;
			d3dRootParam.Constants.RegisterSpace = 0;
			d3dRootParam.Constants.Num32BitValues = bindPointDesc.constantsSize32bitValues;
			cbvRegisterCount++;
		}
		else if (bindPointDesc.type == PipelineBindPointType::ConstantBuffer)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			d3dRootParam.Descriptor.ShaderRegister = cbvRegisterCount;
			d3dRootParam.Descriptor.RegisterSpace = 0;
			d3dRootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE; // TODO: Data volatility
			cbvRegisterCount++;
		}
		else
			; // XEAssertUnreachableCode();

		d3dRootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // TODO: ...
	}

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3dRootSignatureDesc = {};
	d3dRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	d3dRootSignatureDesc.Desc_1_1.pParameters = d3dRootParams;
	d3dRootSignatureDesc.Desc_1_1.NumParameters = rootParameterCount;
	d3dRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	// TODO: D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
	// TODO: D3D12_ROOT_SIGNATURE_FLAG_DENY_*_SHADER_ROOT_ACCESS

	COMPtr<ID3DBlob> d3dRootSignature, d3dError;
	D3D12SerializeVersionedRootSignature(&d3dRootSignatureDesc, d3dRootSignature.initRef(), d3dError.initRef());

	const uint32 headerOffset = 0;
	const uint32 bindPointRecordsOffset = headerOffset + sizeof(ObjectFormat::PipelineLayoutObjectHeader);
	const uint32 rootSignatureOffset = bindPointRecordsOffset + sizeof(ObjectFormat::PipelineBindPointRecord) * desc.bindPointCount;

	const uint32 objectSize = rootSignatureOffset + d3dRootSignature->GetBufferSize();

	result.objectData = SharedDataBufferRef::AllocateBuffer(objectSize);
	byte* objectDataBytes = (byte*)result.objectData.getMutablePointer();

	ObjectFormat::PipelineLayoutObjectHeader header = {};
	header.signature = ObjectFormat::PipelineLayoutObjectSignature;
	header.version = ObjectFormat::PipelineLayoutObjectCurrentVerstion;
	header.objectSize = objectSize;
	header.objectCRC = 0; // TODO: ...

	header.sourceHash = 0; // TODO: ...
	header.bindPointCount = desc.bindPointCount;

	Memory::Copy(objectDataBytes + headerOffset, &header, sizeof(ObjectFormat::PipelineLayoutObjectHeader));
	Memory::Copy(objectDataBytes + bindPointRecordsOffset, bindPointRecords, sizeof(ObjectFormat::PipelineBindPointRecord) * desc.bindPointCount);
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
