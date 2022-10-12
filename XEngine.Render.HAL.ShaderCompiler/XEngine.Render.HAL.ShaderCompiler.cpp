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
#include <XEngine.XStringHash.h>

#include "XEngine.Render.HAL.ShaderCompiler.h"

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
	XAssertUnreachableCode();
	return PipelineBytecodeObjectType::Undefined;
}

static inline ObjectLongHash ComputeObjectLongHash(const void* data, uint32 size)
{
	return ObjectLongHash(CRC64::Compute(data, size));
}

void Object::fillGenericHeaderAndFinalize(uint64 signature)
{
	XAssert(block);
	XAssert(!block->finalized);

	void* data = block + 1;
	XAssert(block->dataSize >= sizeof(GenericObjectHeader));
	GenericObjectHeader& header = *(GenericObjectHeader*)data;
	header.signature = signature;
	header.objectSize = block->dataSize;
	header.objectCRC32 = 0;

	const uint32 crc = CRC32::Compute(data, block->dataSize);
	header.objectCRC32 = crc;

	block->longHash = ComputeObjectLongHash(block + 1, block->dataSize);
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
	XAssert(block && !block->finalized);

	block->longHash = ComputeObjectLongHash(block + 1, block->dataSize);
	block->finalized = true;

	// TODO: Check object header itself
}

Object Object::clone() const
{
	XAssert(block && block->finalized);
	Atomics::Increment(block->referenceCount);

	Object newObject;
	newObject.block = block;
	return newObject;
}

uint32 Object::getCRC32() const
{
	XAssert(block);
	XAssert(block->finalized);

	void* data = block + 1;
	XAssert(block->dataSize >= sizeof(GenericObjectHeader));
	GenericObjectHeader& header = *(GenericObjectHeader*)data;

	return header.objectCRC32;
}

Object Object::Create(uint32 size)
{
	XAssert(size);
	Object object;
	object.block = (BlockHeader*)SystemHeapAllocator::Allocate(sizeof(BlockHeader) + size);
	object.block->referenceCount = 1;
	object.block->dataSize = size;
	object.block->longHash = 0;
	object.block->finalized = false;
	return object;
}

bool CompiledPipelineLayout::findBindingMetadata(uint32 bindingNameXSH, BindingMetadata& result) const
{
	const PipelineLayoutObjectHeader& header = *(PipelineLayoutObjectHeader*)object.getData();
	const PipelineBindingRecord* bindingRecords = (PipelineBindingRecord*)(&header + 1);

	for (uint8 i = 0; i < header.bindingCount; i++)
	{
		if (bindingRecords[i].nameXSH == bindingNameXSH)
		{
			result = bindingsMetadata[i];
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
	if (!object.isValid())
		return ShaderType::Undefined;

	const PipelineBytecodeObjectHeader& header = *(PipelineBytecodeObjectHeader*)object.getData();
	switch (header.objectType)
	{
		case PipelineBytecodeObjectType::ComputeShader:			return ShaderType::Compute;
		case PipelineBytecodeObjectType::VertexShader:			return ShaderType::Vertex;
		case PipelineBytecodeObjectType::AmplificationShader:	return ShaderType::Amplification;
		case PipelineBytecodeObjectType::MeshShader:			return ShaderType::Mesh;
		case PipelineBytecodeObjectType::PixelShader:			return ShaderType::Pixel;
	}
	XAssertUnreachableCode();
	return ShaderType::Undefined;
}

bool Host::CompileDescriptorSetLayout(Platform platform,
	const DescriptorSetBindingDesc* bindings, uint8 bindingCount,
	CompiledDescriptorSetLayout& result)
{
	result.destroy();

	if (bindingCount > MaxDescriptorSetBindingCount)
		return false;

	// TODO: Check for non-unique binding names, zero name hashes, invalid descriptor types.

	const uint32 headerOffset = 0;
	const uint32 bindingNameHashesOffset = headerOffset + sizeof(DescriptorSetLayoutObjectHeader);
	const uint32 bindingDescriptorTypesOffset = bindingNameHashesOffset + sizeof(uint32) * bindingCount;
	const uint32 objectSize = bindingDescriptorTypesOffset + sizeof(DescriptorType) * bindingCount;

	result.object = Object::Create(objectSize);
	{
		byte* objectDataBytes = (byte*)result.object.getMutableData();

		uint32* bindingNameHashes = (uint32*)(objectDataBytes + bindingNameHashesOffset);
		DescriptorType* bindingDescriptorTypes = (DescriptorType*)(objectDataBytes + bindingDescriptorTypesOffset);
		for (uint8 i = 0; i < bindingCount; i++)
		{
			bindingNameHashes[i] = uint32(XStringHash::Compute(bindings[i].name));
			bindingDescriptorTypes[i] = bindings[i].descriptorType;
		}

		const uint32 sourceHash = CRC32::Compute(objectDataBytes + bindingNameHashesOffset,
			(sizeof(uint32) + sizeof(DescriptorType)) * bindingCount);

		DescriptorSetLayoutObjectHeader& header = *(DescriptorSetLayoutObjectHeader*)(objectDataBytes + headerOffset);
		header = {};
		header.generic = {}; // Will be filled later
		header.sourceHash = sourceHash;
		header.bindingCount = bindingCount;
	}
	result.object.fillGenericHeaderAndFinalize(DescriptorSetLayoutObjectSignature);

	return true;
}

bool Host::CompilePipelineLayout(Platform platform,
	const PipelineBindingDesc* bindings, uint8 bindingCount, CompiledPipelineLayout& result)
{
	result.destroy();

	if (bindingCount >= MaxPipelineBindingCount)
		return false;

	// TODO: Check for non-unique binding names.

	D3D12_ROOT_PARAMETER1 d3dRootParams[MaxPipelineBindingCount] = {};
	PipelineBindingRecord bindingRecords[MaxPipelineBindingCount] = {};
	CompiledPipelineLayout::BindingMetadata bindingsMetadata[MaxPipelineBindingCount] = {};

	uint8 rootParameterCount = 0;
	uint8 cbvRegisterCount = 0;
	uint8 srvRegisterCount = 0;
	uint8 uavRegisterCount = 0;
	for (uint32 i = 0; i < bindingCount; i++)
	{
		const PipelineBindingDesc& bindingDesc = bindings[i];
		PipelineBindingRecord& objectBindingRecord = bindingRecords[i];
		CompiledPipelineLayout::BindingMetadata& bindingMetadata = bindingsMetadata[i];

		const uint8 rootParameterIndex = rootParameterCount;
		rootParameterCount++;

		XAssert(!bindingDesc.name.isEmpty());
		const uint32 bindingNameXSH = uint32(XStringHash::Compute(bindingDesc.name));

		// HAL requires this because user can set binding by name hash and zero hash should be invalid input.
		XAssert(bindingNameXSH);

		objectBindingRecord.nameXSH = bindingNameXSH;
		objectBindingRecord.type = bindingDesc.type;
		objectBindingRecord.rootParameterIndex = rootParameterIndex;
		if (bindingDesc.type == PipelineBindingType::Constants)
			objectBindingRecord.constantsSize32bitValues = bindingDesc.constantsSize;

		D3D12_ROOT_PARAMETER1& d3dRootParam = d3dRootParams[rootParameterIndex];

		if (bindingDesc.type == PipelineBindingType::Constants)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			d3dRootParam.Constants.ShaderRegister = cbvRegisterCount;
			d3dRootParam.Constants.RegisterSpace = 0;
			d3dRootParam.Constants.Num32BitValues = bindingDesc.constantsSize;

			bindingMetadata.registerIndex = cbvRegisterCount;
			bindingMetadata.registerType = 'b';

			cbvRegisterCount++;
		}
		else if (bindingDesc.type == PipelineBindingType::ConstantBuffer)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			d3dRootParam.Descriptor.ShaderRegister = cbvRegisterCount;
			d3dRootParam.Descriptor.RegisterSpace = 0;
			d3dRootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

			bindingMetadata.registerIndex = cbvRegisterCount;
			bindingMetadata.registerType = 'b';

			cbvRegisterCount++;
		}
		else if (bindingDesc.type == PipelineBindingType::ReadOnlyBuffer)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			d3dRootParam.Descriptor.ShaderRegister = srvRegisterCount;
			d3dRootParam.Descriptor.RegisterSpace = 0;
			d3dRootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

			bindingMetadata.registerIndex = srvRegisterCount;
			bindingMetadata.registerType = 't';

			srvRegisterCount++;
		}
		else if (bindingDesc.type == PipelineBindingType::ReadWriteBuffer)
		{
			d3dRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			d3dRootParam.Descriptor.ShaderRegister = uavRegisterCount;
			d3dRootParam.Descriptor.RegisterSpace = 0;
			d3dRootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;

			bindingMetadata.registerIndex = uavRegisterCount;
			bindingMetadata.registerType = 'u';

			uavRegisterCount++;
		}
		else if (bindingDesc.type == PipelineBindingType::DescriptorSet)
		{
			XAssertUnreachableCode(); // Not implemented.
		}
		else
			XAssertUnreachableCode();

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
	const uint32 bindingRecordsOffset = headerOffset + sizeof(PipelineLayoutObjectHeader);
	const uint32 rootSignatureOffset = bindingRecordsOffset + sizeof(PipelineBindingRecord) * bindingCount;
	const uint32 objectSize = rootSignatureOffset + uint32(d3dRootSignature->GetBufferSize());

	result.object = Object::Create(objectSize);
	{
		byte* objectDataBytes = (byte*)result.object.getMutableData();

		PipelineLayoutObjectHeader& header = *(PipelineLayoutObjectHeader*)(objectDataBytes + headerOffset);
		header = {};
		header.generic = {}; // Will be filled later
		header.sourceHash = 0; // TODO: ...
		header.bindingCount = bindingCount;

		memoryCopy(objectDataBytes + bindingRecordsOffset, bindingRecords, sizeof(PipelineBindingRecord) * bindingCount);
		memoryCopy(objectDataBytes + rootSignatureOffset, d3dRootSignature->GetBufferPointer(), d3dRootSignature->GetBufferSize());
	}
	result.object.fillGenericHeaderAndFinalize(PipelineLayoutObjectSignature);

	// Fill compiled object metadata
	memoryCopy(result.bindingsMetadata, bindingsMetadata,
		sizeof(CompiledPipelineLayout::BindingMetadata) * bindingCount);

	return true;
}

bool Host::CompileShader(Platform platform, const CompiledPipelineLayout& pipelineLayout,
	const char* source, uint32 sourceLength, ShaderType shaderType, const char* displayedShaderFilename,
	const char* entryPointName, CompiledShader& result)
{
	result.destroy();

	XAssert(pipelineLayout.isInitialized());

	// TODO: Tidy this when more convert utils is ready.
	wchar shaderNameW[64];
	const uintptr shaderNameLength = TextConvertASCIIToWide(displayedShaderFilename, shaderNameW, countof(shaderNameW) - 1);
	XAssert(shaderNameLength < countof(shaderNameW));
	shaderNameW[shaderNameLength] = L'\0';

	wchar entryPointNameW[64];
	const uintptr entryPointNameLength = TextConvertASCIIToWide(entryPointName, entryPointNameW, countof(entryPointNameW) - 1);
	XAssert(entryPointNameLength < countof(entryPointNameW));
	entryPointNameW[entryPointNameLength] = L'\0';

	// Patch source code substituting bindings
	DynamicStringASCII patchedSourceString;
	{
		patchedSourceString.reserve(sourceLength);

		MemoryTextReader sourceReader(source, sourceLength);

		for (;;)
		{
			const bool bindingFound = TextForwardToFirstOccurrence(sourceReader, "@binding", patchedSourceString);
			if (!bindingFound)
				break;

			InplaceStringASCIIx64 bindingName;
			TextSkipWhitespaces(sourceReader);
			if (sourceReader.getChar() != '(')
			{
				TextWriteFmtStdOut("error: invalid binding declaration. Expected `(`\n");
				return false;
			}
			if (!TextReadCIdentifier(sourceReader, bindingName))
			{
				TextWriteFmtStdOut("error: invalid binding declaration. Expected binding name identifier\n");
				return false;
			}
			TextSkipWhitespaces(sourceReader);
			if (sourceReader.getChar() != ')')
			{
				TextWriteFmtStdOut("error: invalid binding declaration. Expected `)`\n");
				return false;
			}

			const uint32 bindingNameXSH = uint32(XStringHash::Compute(bindingName.getData()));
			CompiledPipelineLayout::BindingMetadata bindingMetadata = {};
			if (!pipelineLayout.findBindingMetadata(bindingNameXSH, bindingMetadata))
			{
				TextWriteFmtStdOut("error: unknown binding name `", bindingName, "'\n");
				return false;
			}

			TextWriteFmt(patchedSourceString, "register(",
				bindingMetadata.registerType, uint32(bindingMetadata.registerIndex), ')');
		}

		patchedSourceString.compact();
	}

	ComPtr<IDxcCompiler3> dxcCompiler;
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));

	DxcBuffer dxcSourceBuffer = {};
	dxcSourceBuffer.Ptr = patchedSourceString.getData();
	dxcSourceBuffer.Size = patchedSourceString.getLength();
	dxcSourceBuffer.Encoding = CP_UTF8;

	// TODO: Use `ExpandableInplaceArrayList<LPCWSTR, 32>` here when ready.
	using DXCArgsList = ArrayList<LPCWSTR>;
	DXCArgsList dxcArgsList;

	// Build arguments list
	{
		dxcArgsList.pushBack(shaderNameW);
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
		XAssert(dxcProfile);
		dxcArgsList.pushBack(dxcProfile);

		// TODO: Push this as single string ("-Emain" instead of "-E", "main").
		dxcArgsList.pushBack(L"-E");
		dxcArgsList.pushBack(entryPointNameW);
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
		TextWriteFmtStdOut(dxcErrorsBlob->GetStringPointer());
	}

	HRESULT hCompileStatus = E_FAIL;
	dxcResult->GetStatus(&hCompileStatus);

	if (FAILED(hCompileStatus))
		return false;

	ComPtr<IDxcBlob> dxcBytecodeBlob = nullptr;
	dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxcBytecodeBlob), nullptr);
	XAssert(dxcBytecodeBlob != nullptr && dxcBytecodeBlob->GetBufferSize() > 0);

	// Compose compiled object

	const uint32 headerOffset = 0;
	const uint32 bytecodeOffset = headerOffset + sizeof(PipelineBytecodeObjectHeader);
	const uint32 objectSize = uint32(bytecodeOffset + dxcBytecodeBlob->GetBufferSize());

	result.object = Object::Create(objectSize);
	{
		byte* objectDataBytes = (byte*)result.object.getMutableData();

		PipelineBytecodeObjectHeader& header = *(PipelineBytecodeObjectHeader*)(objectDataBytes + headerOffset);
		header = {};
		header.generic = {}; // Will be filled later
		header.pipelineLayoutSourceHash = pipelineLayout.getSourceHash();
		header.objectType = TranslateShaderTypeToPipelineBytecodeObjectType(shaderType);

		memoryCopy(objectDataBytes + bytecodeOffset, dxcBytecodeBlob->GetBufferPointer(), dxcBytecodeBlob->GetBufferSize());
	}
	result.object.fillGenericHeaderAndFinalize(PipelineBytecodeObjectSignature);

	return true;
}

bool Host::CompileGraphicsPipeline(Platform platform, const CompiledPipelineLayout& pipelineLayout,
	const GraphicsPipelineDesc& desc, CompiledGraphicsPipeline& result)
{
	result.destroy();

	if (desc.vertexShader)
		XAssert(desc.vertexShader->getShaderType() == ShaderType::Vertex);
	if (desc.amplificationShader)
		XAssert(desc.amplificationShader->getShaderType() == ShaderType::Amplification);
	if (desc.meshShader)
		XAssert(desc.meshShader->getShaderType() == ShaderType::Mesh);
	if (desc.pixelShader)
		XAssert(desc.pixelShader->getShaderType() == ShaderType::Pixel);

	// Validate enabled shader stages combination
	XAssert((desc.vertexShader != nullptr) != (desc.meshShader != nullptr));
	XAssert(imply(desc.vertexShader, !desc.amplificationShader));

	// Validate render targets
	{
		bool undefinedRenderTargetFound = false;
		for (TexelViewFormat renderTargetFormat : desc.renderTargetsFormats)
		{
			if (undefinedRenderTargetFound)
				XAssert(renderTargetFormat == TexelViewFormat::Undefined);
			else if (renderTargetFormat == TexelViewFormat::Undefined)
				undefinedRenderTargetFound = true;
			else
				ValidateTexelViewFormatValue(renderTargetFormat);
		}
	}

	// TODO:
	//XAssert(ValidateDepthStencilFormatValue(desc.depthStencilFormat));

	Object bytecodeObjects[MaxGraphicsPipelineBytecodeObjectCount];
	uint32 bytecodeObjectsCRC32s[MaxGraphicsPipelineBytecodeObjectCount] = {};
	uint8 bytecodeObjectCount = 0;
	{
		auto pushBytecodeObject =
			[&bytecodeObjects, &bytecodeObjectsCRC32s, &bytecodeObjectCount](const CompiledShader* shader) -> void
		{
			if (!shader)
				return;

			const Object& object = shader->getObject();
			XAssert(object.isValid());
			bytecodeObjects[bytecodeObjectCount] = object.clone();
			bytecodeObjectsCRC32s[bytecodeObjectCount] = object.getCRC32();
			bytecodeObjectCount++;
			XAssert(bytecodeObjectCount <= MaxGraphicsPipelineBytecodeObjectCount);
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

		memoryCopy(baseObject.bytecodeObjectsCRC32s, bytecodeObjectsCRC32s, sizeof(uint32) * bytecodeObjectCount);

		memoryCopy(baseObject.renderTargetFormats, desc.renderTargetsFormats, sizeof(TexelViewFormat) * MaxRenderTargetCount);
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
