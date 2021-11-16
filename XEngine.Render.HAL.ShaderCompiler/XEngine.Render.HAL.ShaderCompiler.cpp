#include <d3d12.h>
//#include <d3d12shader.h>
#include <dxcapi.h>
#include <wrl/client.h>

#include <XLib.Containers.ArrayList.h>
#include <XLib.CRC.h>
#include <XLib.String.h>
#include <XLib.SystemHeapAllocator.h>
#include <XLib.System.Threading.Atomics.h>
#include <XLib.Text.h>

#include <XEngine.Render.HAL.ObjectFormat.h>

#include "XEngine.Render.HAL.ShaderCompiler.h"

#define XEAssert(cond)
#define XEAssertImply(cond0, cond1)
#define XEAssertUnreachableCode()

using namespace Microsoft::WRL;
using namespace XLib;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::HAL::ObjectFormat;
using namespace XEngine::Render::HAL::ShaderCompiler;
using namespace XEngine::Render::HAL::ShaderCompiler::Internal;

static inline PipelineBytecodeObjectType TranslateShaderTypeToPipelineBytecodeObjectType(ShaderType shaderType)
{
	switch (shaderType)
	{
		case ShaderType::Compute:		return PipelineBytecodeObjectType::ComputeShader;
		case ShaderType::Vertex:		return PipelineBytecodeObjectType::VertexShader;
		case ShaderType::Amplification:	return PipelineBytecodeObjectType::AmplificationShader;
		case ShaderType::Mesh:			return PipelineBytecodeObjectType::MeshShader;
		case ShaderType::Pixel:			return PipelineBytecodeObjectType::PixelShader;
	}

	XEAssertUnreachableCode();
}

struct SharedDataBufferRef::BlockHeader
{
	AtomicU32 referenceCount;
	uint32 dataSize;
	uint64 _padding;
};

void* SharedDataBufferRef::getMutablePointer()
{
	XEAssert(!block);
	return block + 1;
}

SharedDataBufferRef SharedDataBufferRef::createReference() const
{
	XEAssert(block);
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

bool CompiledPipelineLayout::findBindPointMetadata(uint32 bindPointNameCRC, BindPointMetadata& result) const
{
	const PipelineLayoutObjectHeader& header = *(PipelineLayoutObjectHeader*)objectData.getData().data;
	const PipelineBindPointRecord* bindPointRecords = (PipelineBindPointRecord*)(&header + 1);

	for (uint8 i = 0; i < header.bindPointCount; i++)
	{
		if (bindPointRecords[i].nameCRC == bindPointNameCRC)
		{
			result = bindPointsMetadata[i];
			return true;
		}
	}
	return false;
}

uint32 CompiledPipelineLayout::getSourceHash() const
{
	return (*(PipelineLayoutObjectHeader*)objectData.getData().data).sourceHash;
}

ShaderType CompiledShader::getShaderType() const
{
	const PipelineBytecodeObjectHeader& header = *(PipelineBytecodeObjectHeader*)objectData.getData().data;
	switch (header.objectType)
	{
		case PipelineBytecodeObjectType::ComputeShader:			return ShaderType::Compute;
		case PipelineBytecodeObjectType::VertexShader:			return ShaderType::Vertex;
		case PipelineBytecodeObjectType::AmplificationShader:	return ShaderType::Amplification;
		case PipelineBytecodeObjectType::MeshShader:			return ShaderType::Mesh;
		case PipelineBytecodeObjectType::PixelShader:			return ShaderType::Pixel;
	}
	XEAssertUnreachableCode();
}

bool Host::CompilePipelineLayout(Platform platform, const PipelineLayoutDesc& desc, CompiledPipelineLayout& result)
{
	result.destroy();

	if (desc.bindPointCount >= MaxPipelineBindPointCount)
		return false;

	// TODO: Check for non-unique bind point names

	D3D12_ROOT_PARAMETER1 d3dRootParams[MaxPipelineBindPointCount] = {};
	PipelineBindPointRecord bindPointRecords[MaxPipelineBindPointCount] = {};
	CompiledPipelineLayout::BindPointMetadata bindPointsMetadata[MaxPipelineBindPointCount] = {};

	uint8 rootParameterCount = 0;
	uint8 cbvRegisterCount = 0;
	uint8 srvRegisterCount = 0;
	uint8 uavRegisterCount = 0;
	for (uint32 i = 0; i < desc.bindPointCount; i++)
	{
		const PipelineBindPointDesc& bindPointDesc = desc.bindPoints[i];
		PipelineBindPointRecord& objectBindPointRecord = bindPointRecords[i];
		CompiledPipelineLayout::BindPointMetadata& bindPointMetadata = bindPointsMetadata[i];

		const uint8 rootParameterIndex = rootParameterCount;
		rootParameterCount++;

		XEAssert(objectBindPointRecord.nameCRC);
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

			bindPointMetadata.registerIndex = cbvRegisterCount;
			bindPointMetadata.registerType = 'b';

			cbvRegisterCount++;
		}
		else if (bindPointDesc.type == PipelineBindPointType::ConstantBuffer)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			d3dRootParam.Descriptor.ShaderRegister = cbvRegisterCount;
			d3dRootParam.Descriptor.RegisterSpace = 0;
			d3dRootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

			bindPointMetadata.registerIndex = cbvRegisterCount;
			bindPointMetadata.registerType = 'b';

			cbvRegisterCount++;
		}
		else if (bindPointDesc.type == PipelineBindPointType::ReadOnlyBuffer)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			d3dRootParam.Descriptor.ShaderRegister = srvRegisterCount;
			d3dRootParam.Descriptor.RegisterSpace = 0;
			d3dRootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

			bindPointMetadata.registerIndex = srvRegisterCount;
			bindPointMetadata.registerType = 't';

			srvRegisterCount++;
		}
		else if (bindPointDesc.type == PipelineBindPointType::ReadWriteBuffer)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			d3dRootParam.Descriptor.ShaderRegister = uavRegisterCount;
			d3dRootParam.Descriptor.RegisterSpace = 0;
			d3dRootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

			bindPointMetadata.registerIndex = uavRegisterCount;
			bindPointMetadata.registerType = 'u';

			uavRegisterCount++;
		}
		else
			XEAssertUnreachableCode();

		d3dRootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // TODO: ...
	}

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3dRootSignatureDesc = {};
	d3dRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	d3dRootSignatureDesc.Desc_1_1.pParameters = d3dRootParams;
	d3dRootSignatureDesc.Desc_1_1.NumParameters = rootParameterCount;
	d3dRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	// TODO: D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
	// TODO: D3D12_ROOT_SIGNATURE_FLAG_DENY_*_SHADER_ROOT_ACCESS

	ComPtr<ID3DBlob> d3dRootSignature;
	ComPtr<ID3DBlob> d3dError;
	D3D12SerializeVersionedRootSignature(&d3dRootSignatureDesc, &d3dRootSignature, &d3dError);

	// Compose compiled object

	const uint32 headerOffset = 0;
	const uint32 bindPointRecordsOffset = headerOffset + sizeof(PipelineLayoutObjectHeader);
	const uint32 rootSignatureOffset = bindPointRecordsOffset + sizeof(PipelineBindPointRecord) * desc.bindPointCount;
	const uint32 objectSize = rootSignatureOffset + d3dRootSignature->GetBufferSize();

	result.objectData = SharedDataBufferRef::AllocateBuffer(objectSize);
	byte* objectDataBytes = (byte*)result.objectData.getMutablePointer();

	PipelineLayoutObjectHeader& header = *(PipelineLayoutObjectHeader*)(objectDataBytes + headerOffset);
	header = {};
	header.generic.signature = PipelineLayoutObjectSignature;
	header.generic.objectSize = objectSize;
	header.generic.objectCRC = 0; // Will be filled later
	header.sourceHash = 0; // TODO: ...
	header.bindPointCount = desc.bindPointCount;

	Memory::Copy(objectDataBytes + bindPointRecordsOffset, bindPointRecords, sizeof(PipelineBindPointRecord) * desc.bindPointCount);
	Memory::Copy(objectDataBytes + rootSignatureOffset, d3dRootSignature->GetBufferPointer(), d3dRootSignature->GetBufferSize());

	header.generic.objectCRC = CRC32::Compute(objectDataBytes, objectSize);

	// Fill compiled object metadata
	Memory::Copy(result.bindPointsMetadata, bindPointsMetadata,
		sizeof(CompiledPipelineLayout::BindPointMetadata) * desc.bindPointCount);

	return true;
}

bool Host::CompileShader(Platform platform, const CompiledPipelineLayout& pipelineLayout,
	ShaderType shaderType, const char* source, uint32 sourceLength, CompiledShader& result)
{
	result.destroy();

	XEAssert(pipelineLayout.isInitialized());

	// Patch source code substituting bindings
	String patchedSourceString;
	{
		patchedSourceString.reserve(sourceLength);

		TextStreamReader sourceReader(source, sourceLength);
		StringTextStreamWriter patchedSourceWriter(patchedSourceString);

		for (;;)
		{
			const bool bindingFound = TextStreamForwardToFirstOccurrence(sourceReader, "@binding", patchedSourceWriter);
			if (!bindingFound)
				break;

			InplaceString<64> bindPointName;
			TextStreamSkipWritespaces(sourceReader);
			if (TextStreamReadCIdentifier(sourceReader, bindPointName) != TextStreamReadFormatStringResult::Success)
				return false;
			TextStreamSkipWritespaces(sourceReader);
			if (sourceReader.get() != ')')
				return false;

			const uint32 bindPointNameCRC = CRC32::Compute(bindPointName.getData(), bindPointName.getLength());
			CompiledPipelineLayout::BindPointMetadata bindPointMetadata = {};
			if (!pipelineLayout.findBindPointMetadata(bindPointNameCRC, bindPointMetadata))
				return false;

			TextStreamWriteFmt(patchedSourceWriter, ": register(",
				bindPointMetadata.registerType, WFmtUDec(bindPointMetadata.registerIndex), ')');
		}

		patchedSourceString.compact();
	}

	ComPtr<IDxcCompiler3> dxcCompiler;
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));

	DxcBuffer dxcSourceBuffer = {};
	dxcSourceBuffer.Ptr = patchedSourceString.getData();
	dxcSourceBuffer.Size = patchedSourceString.getLength();
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
		XEAssert(dxcTargetProfile);
		dxcArgsList.pushBack(dxcProfile);
	}

	ComPtr<IDxcResult> dxcResult;
	const HRESULT hCompileResult = dxcCompiler->Compile(&dxcSourceBuffer,
		dxcArgsList.getData(), dxcArgsList.getSize(), nullptr, IID_PPV_ARGS(&dxcResult));
	if (FAILED(hCompileResult))
		return false;

	ComPtr<IDxcBlobUtf8> dxcErrorsBlob;
	dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&dxcErrorsBlob), nullptr);

	if (dxcErrorsBlob != nullptr && dxcErrorsBlob->GetStringLength() > 0)
	{

	}

	HRESULT hCompileStatus = E_FAIL;
	dxcResult->GetStatus(&hCompileStatus);

	if (FAILED(hCompileStatus))
		return false;

	ComPtr<IDxcBlob> dxcBytecodeBlob = nullptr;
	dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxcBytecodeBlob), nullptr);
	XEAssert(dxcBytecodeBlob != nullptr && dxcBytecodeBlob->GetBufferSize() > 0);

	// Compose compiled object

	const uint32 headerOffset = 0;
	const uint32 bytecodeOffset = headerOffset + sizeof(PipelineBytecodeObjectHeader);
	const uint32 objectSize = bytecodeOffset + dxcBytecodeBlob->GetBufferSize();

	result.objectData = SharedDataBufferRef::AllocateBuffer(objectSize);
	byte* objectDataBytes = (byte*)result.objectData.getMutablePointer();

	PipelineBytecodeObjectHeader& header = *(PipelineBytecodeObjectHeader*)(objectDataBytes + headerOffset);
	header = {};
	header.generic.signature = PipelineBytecodeObjectSignature;
	header.generic.objectSize = objectSize;
	header.generic.objectCRC = 0; // Will be filled later
	header.pipelineLayoutSourceHash = pipelineLayout.getSourceHash();
	header.objectType = TranslateShaderTypeToPipelineBytecodeObjectType(shaderType);

	Memory::Copy(objectDataBytes + bytecodeOffset, dxcBytecodeBlob->GetBufferPointer(), dxcBytecodeBlob->GetBufferSize());

	header.generic.objectCRC = CRC32::Compute(objectDataBytes, objectSize);

	return true;
}

bool Host::CompileGraphicsPipeline(Platform platform, const CompiledPipelineLayout& pipelineLayout,
	const GraphicsPipelineDesc& desc, CompiledGraphicsPipeline& result)
{
	result.destroy();

	XEAssertImply(desc.vertexShader,		desc.vertexShader->isInitialized());
	XEAssertImply(desc.amplificationShader,	desc.amplificationShader->isInitialized());
	XEAssertImply(desc.meshShader,			desc.meshShader->isInitialized());
	XEAssertImply(desc.pixelShader,			desc.vertexShader->isInitialized());

	XEAssertImply(desc.vertexShader,		desc.vertexShader->getShaderType() == ShaderType::Vertex);
	XEAssertImply(desc.amplificationShader,	desc.amplificationShader->getShaderType() == ShaderType::Amplification);
	XEAssertImply(desc.meshShader,			desc.meshShader->getShaderType() == ShaderType::Mesh);
	XEAssertImply(desc.pixelShader,			desc.vertexShader->getShaderType() == ShaderType::Pixel);

	// Validate enabled shader stages combination
	XEAssert((desc.vertexShader != nullptr) ^ (desc.meshShader != nullptr));
	XEAssert(desc.vertexShader, !desc.amplificationShader);

	// Validate render targets
	{
		bool undefinedRenderTargetFound = false;
		for (TexelFormat renderTargetFormat : desc.renderTargetsFormats)
		{
			if (undefinedRenderTargetFound)
				XEAssert(renderTargetFormat == TexelFormat::Undefined);
			else if (renderTargetFormat == TexelFormat::Undefined)
				undefinedRenderTargetFound = true;
			else
				ValidateTexelFormatValue(renderTargetFormat);
		}
	}

	XEAssert(ValidateTexelFormatValue(desc.depthStencilFormat));

	SharedDataBufferRef bytecodeObjects[MaxGraphicsPipelineBytecodeObjectCount];
	uint32 bytecodeObjectsCRCs[MaxGraphicsPipelineBytecodeObjectCount] = {};
	uint8 bytecodeObjectCount = 0;
	{
		auto pushBytecodeObject =
			[&bytecodeObjects, &bytecodeObjectsCRCs, &bytecodeObjectCount](const CompiledShader* shader) -> void
		{
			if (!shader)
				return;
			bytecodeObjects[bytecodeObjectCount] = shader->objectData.createReference();
			bytecodeObjectsCRCs[bytecodeObjectCount] = shader->getObjectCRC();
			bytecodeObjectCount++;
			XEAssert(bytecodeObjectCount <= MaxGraphicsPipelineBytecodeObjectCount);
		};

		pushBytecodeObject(desc.vertexShader);
		pushBytecodeObject(desc.amplificationShader);
		pushBytecodeObject(desc.meshShader);
		pushBytecodeObject(desc.pixelShader);
	}

	result.baseObjectData = SharedDataBufferRef::AllocateBuffer(sizeof(GraphicsPipelineBaseObject));
	{
		GraphicsPipelineBaseObject& baseObject =
			*(GraphicsPipelineBaseObject*)result.baseObjectData.getMutablePointer();
		baseObject = {};
		baseObject.generic.signature = GraphicsPipelineBaseObjectSignature;
		baseObject.generic.objectSize = sizeof(GraphicsPipelineBaseObject);
		baseObject.generic.objectCRC = 0; // Will be filled later

		baseObject.pipelineLayoutSourceHash = pipelineLayout.getSourceHash();
		Memory::Copy(baseObject.bytecodeObjectsCRCs, bytecodeObjectsCRCs, sizeof(uint32) * bytecodeObjectCount);

		Memory::Copy(baseObject.renderTargetFormats, desc.renderTargetsFormats, sizeof(TexelFormat) * MaxRenderTargetCount);
		baseObject.depthStencilFormat = desc.depthStencilFormat;

		baseObject.enabledShaderStages.vertex			= desc.vertexShader			!= nullptr;
		baseObject.enabledShaderStages.amplification	= desc.amplificationShader	!= nullptr;
		baseObject.enabledShaderStages.mesh				= desc.meshShader			!= nullptr;
		baseObject.enabledShaderStages.pixel			= desc.pixelShader			!= nullptr;

		baseObject.generic.objectCRC = CRC32::Compute(baseObject);
	}

	for (uint8 i = 0; i < bytecodeObjectCount; i++)
		result.bytecodeObjectsData[i] = asRValue(bytecodeObjects[i]);
	result.bytecodeObjectCount = bytecodeObjectCount;

	return true;
}
