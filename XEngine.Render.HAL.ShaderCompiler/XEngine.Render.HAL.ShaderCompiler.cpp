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

static inline ObjectHash CalculateObjectHash(const void* data, uint32 size)
{
	ObjectHash result = {};
	result.a = CRC64::Compute(data, size, 0xD2D6'86F0'85FF'DA5Dull);
	result.b = CRC64::Compute(data, size, 0x632F'54B8'E9F8'FC2Dull);
	return result;
}

void Object::fillGenericHeaderAndFinalize(uint64 signature)
{
	XEAssert(block);
	XEAssert(!block->finalized);

	void* data = block + 1;
	XEAssert(block->dataSize >= sizeof(ObjectFormat::GenericObjectHeader));
	ObjectFormat::GenericObjectHeader& header = *(ObjectFormat::GenericObjectHeader*)data;
	header.signature = signature;
	header.objectSize = block->dataSize;
	header.objectCRC = 0;

	const uint32 crc = CRC32::Compute(data, block->dataSize);
	header.objectCRC = crc;

	block->hash = CalculateObjectHash(block + 1, block->dataSize);
	block->finalized = true;
}

Object::~Object()
{
	if (!block)
		return;

	const uint32 newReferenceCount = Atomics::Decrement(block->referenceCount);
	if (newReferenceCount == 0)
		SystemHeapAllocator::Release(block);
	block = nullptr;
}

void Object::finalize()
{
	XEAssert(block);
	XEAssert(!block->finalized);

	block->hash = CalculateObjectHash(block + 1, block->dataSize);
	block->finalized = true;

	// TODO: Check object header itself
}

Object Object::clone() const
{
	XEAssert(block);
	XEAssert(block->finalized);
	Atomics::Increment(block->referenceCount);

	Object newObject;
	newObject.block = block;
	return newObject;
}

Object Object::Create(uint32 size)
{
	XEAssert(size);
	Object object;
	object.block = (BlockHeader*)SystemHeapAllocator::Allocate(sizeof(BlockHeader) + size);
	object.block->referenceCount = 1;
	object.block->dataSize = size;
	object.block->hash = {};
	object.block->finalized = false;
	return object;
}

bool CompiledPipelineLayout::findBindPointMetadata(uint32 bindPointNameCRC, BindPointMetadata& result) const
{
	const PipelineLayoutObjectHeader& header = *(PipelineLayoutObjectHeader*)object.getData();
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
	return (*(PipelineLayoutObjectHeader*)object.getData()).sourceHash;
}

ShaderType CompiledShader::getShaderType() const
{
	const PipelineBytecodeObjectHeader& header = *(PipelineBytecodeObjectHeader*)object.getData();
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

bool Host::CompilePipelineLayout(Platform platform,
	const PipelineBindPointDesc* bindPoints, const uint8 bindPointCount, CompiledPipelineLayout& result)
{
	result.destroy();

	if (bindPointCount >= MaxPipelineBindPointCount)
		return false;

	// TODO: Check for non-unique bind point names

	D3D12_ROOT_PARAMETER1 d3dRootParams[MaxPipelineBindPointCount] = {};
	PipelineBindPointRecord bindPointRecords[MaxPipelineBindPointCount] = {};
	CompiledPipelineLayout::BindPointMetadata bindPointsMetadata[MaxPipelineBindPointCount] = {};

	uint8 rootParameterCount = 0;
	uint8 cbvRegisterCount = 0;
	uint8 srvRegisterCount = 0;
	uint8 uavRegisterCount = 0;
	for (uint32 i = 0; i < bindPointCount; i++)
	{
		const PipelineBindPointDesc& bindPointDesc = bindPoints[i];
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
	const uint32 rootSignatureOffset = bindPointRecordsOffset + sizeof(PipelineBindPointRecord) * bindPointCount;
	const uint32 objectSize = rootSignatureOffset + d3dRootSignature->GetBufferSize();

	result.object = Object::Create(objectSize);
	{
		byte* objectDataBytes = (byte*)result.object.getMutableData();

		PipelineLayoutObjectHeader& header = *(PipelineLayoutObjectHeader*)(objectDataBytes + headerOffset);
		header = {};
		header.generic = {}; // Will be filled later
		header.sourceHash = 0; // TODO: ...
		header.bindPointCount = bindPointCount;

		Memory::Copy(objectDataBytes + bindPointRecordsOffset, bindPointRecords, sizeof(PipelineBindPointRecord) * bindPointCount);
		Memory::Copy(objectDataBytes + rootSignatureOffset, d3dRootSignature->GetBufferPointer(), d3dRootSignature->GetBufferSize());
	}
	result.object.fillGenericHeaderAndFinalize(PipelineLayoutObjectSignature);

	// Fill compiled object metadata
	Memory::Copy(result.bindPointsMetadata, bindPointsMetadata,
		sizeof(CompiledPipelineLayout::BindPointMetadata) * bindPointCount);

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

	result.object = Object::Create(objectSize);
	{
		byte* objectDataBytes = (byte*)result.object.getMutableData();

		PipelineBytecodeObjectHeader& header = *(PipelineBytecodeObjectHeader*)(objectDataBytes + headerOffset);
		header = {};
		header.generic = {}; // Will be filled later
		header.pipelineLayoutSourceHash = pipelineLayout.getSourceHash();
		header.objectType = TranslateShaderTypeToPipelineBytecodeObjectType(shaderType);

		Memory::Copy(objectDataBytes + bytecodeOffset, dxcBytecodeBlob->GetBufferPointer(), dxcBytecodeBlob->GetBufferSize());
	}
	result.object.fillGenericHeaderAndFinalize(PipelineBytecodeObjectSignature);

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
		for (TexelViewFormat renderTargetFormat : desc.renderTargetsFormats)
		{
			if (undefinedRenderTargetFound)
				XEAssert(renderTargetFormat == TexelViewFormat::Undefined);
			else if (renderTargetFormat == TexelViewFormat::Undefined)
				undefinedRenderTargetFound = true;
			else
				ValidateTexelViewFormatValue(renderTargetFormat);
		}
	}

	XEAssert(ValidateTexelFormatValue(desc.depthStencilFormat));

	Object bytecodeObjects[MaxGraphicsPipelineBytecodeObjectCount];
	uint32 bytecodeObjectsCRCs[MaxGraphicsPipelineBytecodeObjectCount] = {};
	uint8 bytecodeObjectCount = 0;
	{
		auto pushBytecodeObject =
			[&bytecodeObjects, &bytecodeObjectsCRCs, &bytecodeObjectCount](const CompiledShader* shader) -> void
		{
			if (!shader)
				return;

			const Object& object = shader->getObject();
			XEAssert(object.isValid())
			bytecodeObjects[bytecodeObjectCount] = object.clone();
			bytecodeObjectsCRCs[bytecodeObjectCount] = object.getCRC();
			bytecodeObjectCount++;
			XEAssert(bytecodeObjectCount <= MaxGraphicsPipelineBytecodeObjectCount);
		};

		pushBytecodeObject(desc.vertexShader);
		pushBytecodeObject(desc.amplificationShader);
		pushBytecodeObject(desc.meshShader);
		pushBytecodeObject(desc.pixelShader);
	}

	result.baseObject = Object::Create(sizeof(GraphicsPipelineBaseObject));
	{
		GraphicsPipelineBaseObject& baseObject =
			*(GraphicsPipelineBaseObject*)result.baseObject.getMutableData();
		baseObject = {};
		baseObject.generic = {}; // Will be filled later
		baseObject.pipelineLayoutSourceHash = pipelineLayout.getSourceHash();

		Memory::Copy(baseObject.bytecodeObjectsCRCs, bytecodeObjectsCRCs, sizeof(uint32) * bytecodeObjectCount);

		Memory::Copy(baseObject.renderTargetFormats, desc.renderTargetsFormats, sizeof(TexelViewFormat) * MaxRenderTargetCount);
		baseObject.depthStencilFormat = desc.depthStencilFormat;

		baseObject.enabledShaderStages.vertex			= desc.vertexShader			!= nullptr;
		baseObject.enabledShaderStages.amplification	= desc.amplificationShader	!= nullptr;
		baseObject.enabledShaderStages.mesh				= desc.meshShader			!= nullptr;
		baseObject.enabledShaderStages.pixel			= desc.pixelShader			!= nullptr;
	}
	result.baseObject.fillGenericHeaderAndFinalize(GraphicsPipelineBaseObjectSignature);

	for (uint8 i = 0; i < bytecodeObjectCount; i++)
		result.bytecodeObjects[i].moveFrom(bytecodeObjects[i]);
	result.bytecodeObjectCount = bytecodeObjectCount;

	return true;
}
