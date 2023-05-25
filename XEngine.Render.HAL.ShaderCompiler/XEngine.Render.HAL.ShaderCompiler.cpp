#include <d3d12.h>
//#include <d3d12shader.h>
#include <dxcapi.h>
#include <wrl/client.h>

#include <XLib.Containers.ArrayList.h>
#include <XLib.String.h>
#include <XLib.SystemHeapAllocator.h>
#include <XLib.System.Threading.Atomics.h>
#include <XLib.Text.h>

#include <XEngine.Render.HAL.BlobFormat.h>
#include <XEngine.XStringHash.h>

#include "XEngine.Render.HAL.ShaderCompiler.h"

// TODO: Check for non-unique binding names and hash collisions.
// NOTE: Binding name hash collisions should be checked for lower 32 bits as only these are used in runtime.

using namespace Microsoft::WRL;
using namespace XLib;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::HAL::ShaderCompiler;

static inline D3D12_DESCRIPTOR_RANGE_TYPE TranslateDescriptorTypeToD3D12DescriptorRangeType(DescriptorType type)
{
	switch (type)
	{
		case DescriptorType::ReadOnlyBuffer:
		case DescriptorType::ReadOnlyTexture:
		case DescriptorType::RaytracingAccelerationStructure:
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

		case DescriptorType::ReadWriteBuffer:
		case DescriptorType::ReadWriteTexture:
			return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	}
	XAssertUnreachableCode();
	return D3D12_DESCRIPTOR_RANGE_TYPE(-1);
}

static inline char TranslateDescriptorTypeToShaderRegisterType(DescriptorType type)
{
	switch (type)
	{
		case DescriptorType::ReadOnlyBuffer:
		case DescriptorType::ReadOnlyTexture:
		case DescriptorType::RaytracingAccelerationStructure:
			return 't';

		case DescriptorType::ReadWriteBuffer:
		case DescriptorType::ReadWriteTexture:
			return 'u';
	}
	XAssertUnreachableCode();
	return 0;
}

static inline char TranslatePipelineBindingTypeToShaderRegisterType(PipelineBindingType type)
{
	switch (type)
	{
		case PipelineBindingType::Constants:
		case PipelineBindingType::ConstantBuffer:
			return 'b';

		case PipelineBindingType::ReadOnlyBuffer:
			return 't';

		case PipelineBindingType::ReadWriteBuffer:
			return 'u';
	}
	XAssertUnreachableCode();
	return 0;
}

static inline BlobFormat::BytecodeBlobType TranslateShaderTypeToBytecodeBlobType(ShaderType shaderType)
{
	switch (shaderType)
	{
		case ShaderType::Compute:		return BlobFormat::BytecodeBlobType::ComputeShader;
		case ShaderType::Vertex:		return BlobFormat::BytecodeBlobType::VertexShader;
		case ShaderType::Amplification:	return BlobFormat::BytecodeBlobType::AmplificationShader;
		case ShaderType::Mesh:			return BlobFormat::BytecodeBlobType::MeshShader;
		case ShaderType::Pixel:			return BlobFormat::BytecodeBlobType::PixelShader;
	}
	XAssertUnreachableCode();
	return BlobFormat::BytecodeBlobType::Undefined;
}

// Blob ////////////////////////////////////////////////////////////////////////////////////////////

Blob::~Blob()
{
	if (!data)
		return;

	// TODO: This is not thread-safe. Fix it.

	const uint32 newReferenceCount = Atomics::Decrement(data->referenceCount);
	if (newReferenceCount == 0)
		SystemHeapAllocator::Release(data);
	data = nullptr;
}

Blob Blob::addReference() const
{
	XAssert(data);
	Atomics::Increment(data->referenceCount);

	Blob newBlobRef;
	newBlobRef.data = data;
	return newBlobRef;
}

void* Blob::Create(uint32 size, Blob& blob)
{
	XAssert(size);

	blob.destroy();
	blob.data = (Preamble*)SystemHeapAllocator::Allocate(sizeof(Preamble) + size);
	blob.data->referenceCount = 1;
	blob.data->dataSize = size;

	return blob.data + 1;
}

// Host ////////////////////////////////////////////////////////////////////////////////////////////

bool Host::CompileDescriptorSetLayout(Platform platform,
	const DescriptorSetNestedBindingDesc* bindings, uint16 bindingCount, Blob& resultBlob)
{
	resultBlob.destroy();

	if (bindingCount > MaxDescriptorSetNestedBindingCount)
		return false;

	// TODO: Check for non-unique binding names, hash collisions, zero name hashes, invalid descriptor types.

	BlobFormat::DescriptorSetLayoutBlobWriter blobWriter;

	blobWriter.initialize(bindingCount);
	{
		const uint32 blobSize = blobWriter.getMemoryBlockSize();
		void* blobMemoryBlock = Blob::Create(blobSize, resultBlob);
		blobWriter.setMemoryBlock(blobMemoryBlock, blobSize);
	}

	for (uint16 i = 0; i < bindingCount; i++)
	{
		const DescriptorSetNestedBindingDesc& srcBindingDesc = bindings[i];

		if (srcBindingDesc.name.isEmpty())
			return false;
		const uint64 nameXSH = XStringHash::Compute(srcBindingDesc.name);
		XAssert(srcBindingDesc.descriptorCount == 1);

		BlobFormat::DescriptorSetNestedBindingInfo dstBindingInfo = {};
		dstBindingInfo.nameXSH = nameXSH;
		dstBindingInfo.descriptorCount = srcBindingDesc.descriptorCount;
		dstBindingInfo.descriptorType = srcBindingDesc.descriptorType;

		blobWriter.writeBinding(i, dstBindingInfo);
	}

	const uint32 sourceHash = 0; // TODO: ...
	blobWriter.finalize(sourceHash);

	return true;
}

bool Host::CompilePipelineLayout(Platform platform,
	const PipelineBindingDesc* bindings, uint16 bindingCount,
	Blob& resultBlob, Blob& resultMetadataBlob)
{
	resultBlob.destroy();
	resultMetadataBlob.destroy();

	if (bindingCount > MaxPipelineBindingCount)
		return false;

	BlobFormat::PipelineLayoutBlobWriter blobWriter;
	BlobFormat::PipelineLayoutMetadataBlobWriter metadataBlobWriter;

	blobWriter.beginInitialization();
	metadataBlobWriter.beginInitialization();

	InplaceArrayList<D3D12_ROOT_PARAMETER1, MaxPipelineBindingCount> d3dRootParams;
	InplaceArrayList<D3D12_DESCRIPTOR_RANGE1, 128> d3dDescriptorRanges; // TODO: Use ExpandableInplaceArrayList here.

	uint16 shaderRegisterCounter = 0;
	for (uint16 pipelineBindingIndex = 0; pipelineBindingIndex < bindingCount; pipelineBindingIndex++)
	{
		const PipelineBindingDesc& scrPipelinBindingDesc = bindings[pipelineBindingIndex];

		if (scrPipelinBindingDesc.name.isEmpty())
			return false;
		const uint64 pipelineBindingNameXSH = XStringHash::Compute(scrPipelinBindingDesc.name);

		const uint16 rootParameterIndex = d3dRootParams.getSize();
		D3D12_ROOT_PARAMETER1& d3dRootParameter = d3dRootParams.emplaceBack();
		d3dRootParameter = {};

		BlobFormat::PipelineBindingInfo dstPipelineBindingInfo = {};
		dstPipelineBindingInfo.nameXSH = pipelineBindingNameXSH; // TODO: We should check collision for lower 32 bits.
		dstPipelineBindingInfo.platformBindingIndex = rootParameterIndex;
		dstPipelineBindingInfo.type = scrPipelinBindingDesc.type;

		if (scrPipelinBindingDesc.type == PipelineBindingType::Constants)
		{
			const uint16 shaderRegister = shaderRegisterCounter;
			shaderRegisterCounter++;

			dstPipelineBindingInfo.constantsSize = scrPipelinBindingDesc.constantsSize;

			blobWriter.addPipelineBinding(dstPipelineBindingInfo);
			metadataBlobWriter.addGenericPipelineBinding(shaderRegister);

			d3dRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			d3dRootParameter.Constants.ShaderRegister = shaderRegister;
			d3dRootParameter.Constants.RegisterSpace = 0;
			d3dRootParameter.Constants.Num32BitValues = scrPipelinBindingDesc.constantsSize;
		}
		else if (scrPipelinBindingDesc.type == PipelineBindingType::ConstantBuffer)
		{
			const uint16 shaderRegister = shaderRegisterCounter;
			shaderRegisterCounter++;

			blobWriter.addPipelineBinding(dstPipelineBindingInfo);
			metadataBlobWriter.addGenericPipelineBinding(shaderRegister);

			d3dRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			d3dRootParameter.Descriptor.ShaderRegister = shaderRegister;
			d3dRootParameter.Descriptor.RegisterSpace = 0;
			d3dRootParameter.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		}
		else if (scrPipelinBindingDesc.type == PipelineBindingType::ReadOnlyBuffer)
		{
			const uint16 shaderRegister = shaderRegisterCounter;
			shaderRegisterCounter++;

			blobWriter.addPipelineBinding(dstPipelineBindingInfo);
			metadataBlobWriter.addGenericPipelineBinding(shaderRegister);

			d3dRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			d3dRootParameter.Descriptor.ShaderRegister = shaderRegister;
			d3dRootParameter.Descriptor.RegisterSpace = 0;
			d3dRootParameter.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		}
		else if (scrPipelinBindingDesc.type == PipelineBindingType::ReadWriteBuffer)
		{
			const uint16 shaderRegister = shaderRegisterCounter;
			shaderRegisterCounter++;

			blobWriter.addPipelineBinding(dstPipelineBindingInfo);
			metadataBlobWriter.addGenericPipelineBinding(shaderRegister);

			d3dRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			d3dRootParameter.Descriptor.ShaderRegister = shaderRegister;
			d3dRootParameter.Descriptor.RegisterSpace = 0;
			d3dRootParameter.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		}
		else if (scrPipelinBindingDesc.type == PipelineBindingType::DescriptorSet)
		{
			BlobFormat::DescriptorSetLayoutBlobReader dslBlobReader;
			XAssert(scrPipelinBindingDesc.compiledDescriptorSetLayout);
			if (!dslBlobReader.open(
				scrPipelinBindingDesc.compiledDescriptorSetLayout->getData(),
				scrPipelinBindingDesc.compiledDescriptorSetLayout->getSize()))
				return false;

			dstPipelineBindingInfo.descriptorSetLayoutSourceHash = dslBlobReader.getSourceHash();

			blobWriter.addPipelineBinding(dstPipelineBindingInfo);
			metadataBlobWriter.addDescriptorSetPipelineBindingAndBeginAddingNestedBindings();

			const uint16 descriptorSetBaseShaderRegister = shaderRegisterCounter;
			const uint32 descriptorSetBaseRange = d3dDescriptorRanges.getSize();

			D3D12_DESCRIPTOR_RANGE1 d3dCurrentRange = {};
			bool currentRangeInitialized = false;

			uint16 descriptorOffsetCounter = 0;
			XAssert(dslBlobReader.getBindingCount() > 0);
			for (uint32 i = 0; i < dslBlobReader.getBindingCount(); i++)
			{
				const BlobFormat::DescriptorSetNestedBindingInfo nestedBinding = dslBlobReader.getBinding(i);

				const uint16 descriptorOffset = descriptorOffsetCounter;
				descriptorOffsetCounter += nestedBinding.descriptorCount;

				const uint16 shaderRegister = descriptorSetBaseShaderRegister + descriptorOffset;

				metadataBlobWriter.addDescriptorSetNestedBinding(nestedBinding, shaderRegister);

				const D3D12_DESCRIPTOR_RANGE_TYPE d3dRequiredRangeType =
					TranslateDescriptorTypeToD3D12DescriptorRangeType(nestedBinding.descriptorType);
				if (!currentRangeInitialized || d3dCurrentRange.RangeType != d3dRequiredRangeType)
				{
					// Finalize previous range and start new one.
					if (currentRangeInitialized)
					{
						// Submit current range.
						d3dCurrentRange.NumDescriptors =
							descriptorOffset - d3dCurrentRange.OffsetInDescriptorsFromTableStart;

						XAssert(!d3dDescriptorRanges.isFull()); // TODO: XMasterAssert
						d3dDescriptorRanges.pushBack(d3dCurrentRange);
					}

					d3dCurrentRange = {};
					d3dCurrentRange.RangeType = d3dRequiredRangeType;
					d3dCurrentRange.NumDescriptors = 0;
					d3dCurrentRange.BaseShaderRegister = shaderRegister;
					d3dCurrentRange.RegisterSpace = 0;
					d3dCurrentRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
					d3dCurrentRange.OffsetInDescriptorsFromTableStart = descriptorOffset;
					currentRangeInitialized = true;
				}
			}

			metadataBlobWriter.endAddingDescriptorSetNestedBindings();

			XAssert(currentRangeInitialized);
			{
				// Submit final range.
				d3dCurrentRange.NumDescriptors =
					descriptorOffsetCounter - d3dCurrentRange.OffsetInDescriptorsFromTableStart;

				XAssert(!d3dDescriptorRanges.isFull()); // TODO: XMasterAssert
				d3dDescriptorRanges.pushBack(d3dCurrentRange);
			}

			const uint16 descriptorCount = descriptorOffsetCounter;
			shaderRegisterCounter += descriptorCount;

			d3dRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			d3dRootParameter.DescriptorTable.NumDescriptorRanges = d3dDescriptorRanges.getSize() - descriptorSetBaseRange;
			d3dRootParameter.DescriptorTable.pDescriptorRanges = &d3dDescriptorRanges[descriptorSetBaseRange];
		}
		else
			XAssertUnreachableCode();

		d3dRootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // TODO: ...
	}

	XAssert(blobWriter.getPipelineBingingCount() == bindingCount);
	XAssert(metadataBlobWriter.getPipelineBingingCount() == bindingCount);

	// Compile root signature.
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3dRootSignatureDesc = {};
	d3dRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	d3dRootSignatureDesc.Desc_1_1.pParameters = d3dRootParams.getData();
	d3dRootSignatureDesc.Desc_1_1.NumParameters = d3dRootParams.getSize();
	d3dRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
	// TODO: D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
	// TODO: D3D12_ROOT_SIGNATURE_FLAG_DENY_*_SHADER_ROOT_ACCESS

	ComPtr<ID3DBlob> d3dRootSignature;
	ComPtr<ID3DBlob> d3dError;
	D3D12SerializeVersionedRootSignature(&d3dRootSignatureDesc, &d3dRootSignature, &d3dError);
	// TODO: Check HRESULT.

	const uint32 sourceHash = 0; // TODO: ...

	blobWriter.endInitialization(uint32(d3dRootSignature->GetBufferSize()));
	{
		const uint32 blobSize = blobWriter.getMemoryBlockSize();
		void* blobMemoryBlock = Blob::Create(blobSize, resultBlob);
		blobWriter.setMemoryBlock(blobMemoryBlock, blobSize);
	}
	blobWriter.writePlatformData(d3dRootSignature->GetBufferPointer(), uint32(d3dRootSignature->GetBufferSize()));
	blobWriter.finalize(sourceHash);

	metadataBlobWriter.endInitialization();
	{
		const uint32 metadataBlobSize = metadataBlobWriter.getMemoryBlockSize();
		void* metadataBlobMemoryBlock = Blob::Create(metadataBlobSize, resultMetadataBlob);
		metadataBlobWriter.finalizeToMemoryBlock(metadataBlobMemoryBlock, metadataBlobSize, sourceHash);
	}

	return true;
}

bool Host::CompileShader(Platform platform,
	const Blob& compiledPipelineLayoutBlob, const Blob& pipelineLayoutMetadataBlob,
	const char* source, uint32 sourceLength, ShaderType shaderType, const char* displayedShaderFilename,
	const char* entryPointName, Blob& resultBlob)
{
	resultBlob.destroy();

	XAssert(compiledPipelineLayoutBlob.isInitialized());
	XAssert(pipelineLayoutMetadataBlob.isInitialized());

	BlobFormat::PipelineLayoutBlobReader pipelineLayoutBlobReader;
	BlobFormat::PipelineLayoutMetadataBlobReader pipelineLayoutMetadataBlobReader;

	if (!pipelineLayoutBlobReader.open(compiledPipelineLayoutBlob.getData(), compiledPipelineLayoutBlob.getSize()))
		return false;
	if (!pipelineLayoutMetadataBlobReader.open(pipelineLayoutMetadataBlob.getData(), pipelineLayoutMetadataBlob.getSize()))
		return false;

	if (pipelineLayoutBlobReader.getSourceHash() != pipelineLayoutMetadataBlobReader.getSourceHash())
		return false;
	if (pipelineLayoutBlobReader.getPipelineBindingCount() != pipelineLayoutMetadataBlobReader.getPipelineBindingCount())
		return false;

	const uint32 pipelineBindingCount = pipelineLayoutBlobReader.getPipelineBindingCount();

	// Patch source code substituting bindings.
	DynamicStringASCII patchedSourceString;
	{
		patchedSourceString.reserve(sourceLength);

		MemoryTextReader sourceReader(source, sourceLength);

		for (;;)
		{
			const bool bindingFound = TextForwardToFirstOccurrence(sourceReader, "@binding", patchedSourceString);
			if (!bindingFound)
				break;

			InplaceStringASCII<MaxBindingNameLength + 2> pipelineBindingName;
			// NOTE: +2 to store one additional character so we know that name is too long when string is full.

			TextSkipWhitespaces(sourceReader);
			if (sourceReader.getChar() != '(')
			{
				TextWriteFmtStdOut("error: invalid binding declaration: expected `(`\n");
				return false;
			}
			TextSkipWhitespaces(sourceReader);
			if (!TextReadCIdentifier(sourceReader, pipelineBindingName))
			{
				TextWriteFmtStdOut("error: invalid binding declaration: expected binding name identifier\n");
				return false;
			}

			if (pipelineBindingName.isFull())
			{
				TextWriteFmtStdOut("error: binding name is too long\n");
				return false;
			}

			const uint64 pipelineBindingNameXSH = XStringHash::Compute(pipelineBindingName);

			BlobFormat::PipelineBindingInfo pipelineBindingInfo = {};
			uint16 pipelineBindingIndex = 0;
			bool pipelineBindingFound = false;
			for (uint16 i = 0; i < pipelineBindingCount; i++)
			{
				const BlobFormat::PipelineBindingInfo ithBindingInfo = pipelineLayoutBlobReader.getPipelineBinding(i);
				if (pipelineBindingNameXSH == ithBindingInfo.nameXSH)
				{
					pipelineBindingInfo = ithBindingInfo;
					pipelineBindingIndex = i;
					pipelineBindingFound = true;
					break;
				}
			}

			if (!pipelineBindingFound)
			{
				TextWriteFmtStdOut("error: unknown binding name `", pipelineBindingName, "'\n");
				return false;
			}

			char shaderRegisterType = 0;
			uint32 shaderRegister = 0;

			TextSkipWhitespaces(sourceReader);

			const bool isDescriptorSetPipelineBinding = pipelineBindingInfo.type == PipelineBindingType::DescriptorSet;
			if (isDescriptorSetPipelineBinding != pipelineLayoutMetadataBlobReader.isDescriptorSetBinding(pipelineBindingIndex))
				return false;

			if (isDescriptorSetPipelineBinding)
			{
				const uint32 nestedBindingCount =
					pipelineLayoutMetadataBlobReader.getDescriptorSetNestedBindingCount(pipelineBindingIndex);

				if (sourceReader.getChar() != '.')
				{
					TextWriteFmtStdOut("error: invalid binding declaration: expected '.' for nested binding\n");
					return false;
				}

				InplaceStringASCII<MaxBindingNameLength + 2> nestedBindingName;

				TextSkipWhitespaces(sourceReader);
				if (!TextReadCIdentifier(sourceReader, nestedBindingName))
				{
					TextWriteFmtStdOut("error: invalid binding declaration: expected nested binding name identifier\n");
					return false;
				}

				if (nestedBindingName.isFull())
				{
					TextWriteFmtStdOut("error: binding name is too long\n");
					return false;
				}

				const uint64 nestedBindingNameXSH = XStringHash::Compute(nestedBindingName);

				BlobFormat::DescriptorSetNestedBindingMetaInfo nestedBindingInfo = {};
				uint16 nestedBindingIndex = 0;
				bool nestedBindingFound = false;
				for (uint16 i = 0; i < nestedBindingCount; i++)
				{
					const BlobFormat::DescriptorSetNestedBindingMetaInfo ithBindingInfo =
						pipelineLayoutMetadataBlobReader.getDescriptorSetNestedBinding(pipelineBindingIndex, i);
					if (nestedBindingNameXSH == ithBindingInfo.generic.nameXSH)
					{
						nestedBindingInfo = ithBindingInfo;
						nestedBindingIndex = i;
						nestedBindingFound = true;
						break;
					}
				}

				if (!nestedBindingFound)
				{
					TextWriteFmtStdOut("error: unknown nested binding name `", nestedBindingName, "'\n");
					return false;
				}

				shaderRegisterType = TranslateDescriptorTypeToShaderRegisterType(nestedBindingInfo.generic.descriptorType);
				shaderRegister = nestedBindingInfo.shaderRegister;
			}
			else
			{
				shaderRegisterType = TranslatePipelineBindingTypeToShaderRegisterType(pipelineBindingInfo.type);
				shaderRegister = pipelineLayoutMetadataBlobReader.getPipelineBindingShaderRegister(pipelineBindingIndex);
			}

			TextSkipWhitespaces(sourceReader);
			if (sourceReader.getChar() != ')')
			{
				TextWriteFmtStdOut("error: invalid binding declaration: expected `)`\n");
				return false;
			}

			TextWriteFmt(patchedSourceString, "register(", shaderRegisterType, shaderRegister, ')');
		}

		patchedSourceString.compact();
	}

	// TODO: Tidy this when more convert utils is ready.
	wchar shaderNameW[64];
	const uintptr shaderNameLength = TextConvertASCIIToWide(displayedShaderFilename, shaderNameW, countof(shaderNameW) - 1);
	XAssert(shaderNameLength < countof(shaderNameW));
	shaderNameW[shaderNameLength] = L'\0';

	wchar entryPointNameW[64];
	const uintptr entryPointNameLength = TextConvertASCIIToWide(entryPointName, entryPointNameW, countof(entryPointNameW) - 1);
	XAssert(entryPointNameLength < countof(entryPointNameW));
	entryPointNameW[entryPointNameLength] = L'\0';

	// Actual compilation.
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

	// Compose compiled object.
	BlobFormat::BytecodeBlobWriter blobWriter;
	blobWriter.initialize(uint32(dxcBytecodeBlob->GetBufferSize()));
	{
		const uint32 blobSize = blobWriter.getMemoryBlockSize();
		void* blobMemoryBlock = Blob::Create(blobSize, resultBlob);
		blobWriter.setMemoryBlock(blobMemoryBlock, blobSize);
	}
	blobWriter.writeBytecode(dxcBytecodeBlob->GetBufferPointer(), uint32(dxcBytecodeBlob->GetBufferSize()));
	blobWriter.finalize(TranslateShaderTypeToBytecodeBlobType(shaderType), pipelineLayoutBlobReader.getSourceHash());

	return true;
}

bool Host::CompileGraphicsPipeline(Platform platform, const Blob& compiledPipelineLayout,
	const GraphicsPipelineDesc& desc, GraphicsPipelineCompilationResult& result)
{
	result.baseBlob.destroy();
	for (Blob& resultBytecodeBlob : result.bytecodeBlobs)
		resultBytecodeBlob.destroy();

	BlobFormat::PipelineLayoutBlobReader pipelineLayoutBlobReader;
	if (!pipelineLayoutBlobReader.open(compiledPipelineLayout.getData(), compiledPipelineLayout.getSize()))
		return false;
	const uint32 pipelineLayoutSourceHash = pipelineLayoutBlobReader.getSourceHash();

	BlobFormat::GraphicsPipelineBaseBlobWriter baseBlobWriter;
	baseBlobWriter.beginInitialization();

	// Validate and register bytecode blobs.

	auto validateAndRegisterBytecodeBlob =
		[&baseBlobWriter, pipelineLayoutSourceHash]
		(const Blob& bytecodeBlob, BlobFormat::BytecodeBlobType expectedBlobType) -> bool
	{
		BlobFormat::BytecodeBlobReader reader;
		if (!reader.open(bytecodeBlob.getData(), bytecodeBlob.getSize()))
			return false;
		if (reader.getType() != expectedBlobType)
			return false;
		if (reader.getPipelineLayoutSourceHash() != pipelineLayoutSourceHash)
			return false;
		baseBlobWriter.registerBytecodeBlob(expectedBlobType, reader.getChecksum());
		return true;
	};

	InplaceArrayList<Blob, 8> resultBytecodeBlobs;

	if (desc.compiledVertexShader)
	{
		if (desc.compiledMeshShader)
			return false;
		if (!validateAndRegisterBytecodeBlob(*desc.compiledVertexShader, BlobFormat::BytecodeBlobType::VertexShader))
			return false;
		resultBytecodeBlobs.pushBack(desc.compiledVertexShader->addReference());
	}
	if (desc.compiledAmplificationShader)
	{
		if (!desc.compiledMeshShader)
			return false;
		if (!validateAndRegisterBytecodeBlob(*desc.compiledAmplificationShader, BlobFormat::BytecodeBlobType::AmplificationShader))
			return false;
		resultBytecodeBlobs.pushBack(desc.compiledAmplificationShader->addReference());
	}
	if (desc.compiledMeshShader)
	{
		if (desc.compiledVertexShader)
			return false;
		if (!validateAndRegisterBytecodeBlob(*desc.compiledMeshShader, BlobFormat::BytecodeBlobType::MeshShader))
			return false;
		resultBytecodeBlobs.pushBack(desc.compiledMeshShader->addReference());
	}
	if (desc.compiledPixelShader)
	{
		if (!validateAndRegisterBytecodeBlob(*desc.compiledPixelShader, BlobFormat::BytecodeBlobType::PixelShader))
			return false;
		resultBytecodeBlobs.pushBack(desc.compiledPixelShader->addReference());
	}

	XAssert(resultBytecodeBlobs.getSize() <= MaxGraphicsPipelineBytecodeBlobCount);

	// Validate and register render targets.
	{
		bool undefinedRenderTargetFound = false;
		for (TexelViewFormat renderTargetFormat : desc.renderTargetsFormats)
		{
			if (undefinedRenderTargetFound)
				XAssert(renderTargetFormat == TexelViewFormat::Undefined);
			else if (renderTargetFormat == TexelViewFormat::Undefined)
				undefinedRenderTargetFound = true;
			else
			{
				if (!ValidateTexelViewFormatValue(renderTargetFormat))
					return false;
				if (!HAL::DoesTexelViewFormatSupportColorRenderTargetUsage(renderTargetFormat))
					return false;
				baseBlobWriter.addRenderTarget(renderTargetFormat);
			}
		}
	}

	// TODO: `ValidateDepthStencilFormatValue`.
	baseBlobWriter.setDepthStencilFormat(desc.depthStencilFormat);

	baseBlobWriter.endInitialization();
	{
		const uint32 baseBlobSize = baseBlobWriter.getMemoryBlockSize();
		void* baseBlobMemoryBlock = Blob::Create(baseBlobSize, result.baseBlob);
		baseBlobWriter.finalizeToMemoryBlock(baseBlobMemoryBlock, baseBlobSize, pipelineLayoutSourceHash);
	}

	for (uint32 i = 0; i < resultBytecodeBlobs.getSize(); i++)
		result.bytecodeBlobs[i].moveFrom(resultBytecodeBlobs[i]);

	return true;
}
