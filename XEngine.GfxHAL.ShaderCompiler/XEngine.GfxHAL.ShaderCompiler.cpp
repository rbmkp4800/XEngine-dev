#include <d3d12.h>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <wrl/client.h>

#include <XLib.Containers.ArrayList.h>
#include <XLib.String.h>
#include <XLib.SystemHeapAllocator.h>
#include <XLib.System.Threading.Atomics.h>
#include <XLib.Text.h>

#include <XEngine.GfxHAL.BlobFormat.h>
#include <XEngine.XStringHash.h>

#include "XEngine.GfxHAL.ShaderCompiler.h"
#include "XEngine.GfxHAL.ShaderCompiler.HLSLPatcher.h"

using namespace XLib;
using namespace XEngine::GfxHAL;
using namespace XEngine::GfxHAL::ShaderCompiler;

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


// DescriptorSetLayout /////////////////////////////////////////////////////////////////////////////

sint16 DescriptorSetLayout::findBinding(StringViewASCII name) const
{
	for (uint16 i = 0; i < bindingCount; i++)
	{
		if (namesBuffer.getSubString(bindings[i].nameOffset, bindings[i].nameLength) == name)
			return i;
	}
	return -1;
}

DescriptorSetBindingDesc DescriptorSetLayout::getBindingDesc(uint16 bindingIndex) const
{
	XAssert(bindingIndex < bindingCount);
	const BindingDesc& internalDesc = bindings[bindingIndex];

	DescriptorSetBindingDesc result = {};
	result.name = namesBuffer.getSubString(internalDesc.nameOffset, internalDesc.nameLength);
	result.descriptorCount = internalDesc.descriptorCount;
	result.descriptorType = internalDesc.descriptorType;
	return result;
}

uint32 DescriptorSetLayout::getBindingDescriptorOffset(uint16 bindingIndex) const
{
	XAssert(bindingIndex < bindingCount);
	return bindings[bindingIndex].descriptorOffset;
}

uint32 DescriptorSetLayout::getSerializedBlobChecksum() const
{
	return ((BlobFormat::Data::GenericBlobHeader*)getSerializedBlobData())->checksum;
}

DescriptorSetLayoutRef DescriptorSetLayout::Create(const DescriptorSetBindingDesc* bindings, uint16 bindingCount)
{
	// NOTE: I am retarded, so I put all the data in a single heap allocation :autistic_face:

	XAssert(bindingCount <= MaxDescriptorSetBindingCount);

	struct BindingDeducedInfo
	{
		uint64 nameXSH;
		uint32 descriptorOffset;
		uint16 nameOffset;
	};
	BindingDeducedInfo bindingsDeducedInfo[MaxDescriptorSetBindingCount] = {};

	// Generic checks for each binding.
	// Calculate bindings deduced info.
	// Calculate names buffer length.
	// Calculate total descriptor count.
	uint32 namesBufferSize = 0;
	uint32 descriptorCount = 0;
	for (uint16 i = 0; i < bindingCount; i++)
	{
		const DescriptorSetBindingDesc& binding = bindings[i];

		XAssert(!binding.name.isEmpty());
		XAssert(binding.name.getLength() <= MaxDescriptorSetBindingNameLength);
		XAssert(binding.descriptorCount == 1);
		// TODO: Validate `binding.descriptorType` value.

		bindingsDeducedInfo[i].nameXSH = XSH::Compute(binding.name);
		bindingsDeducedInfo[i].descriptorOffset = descriptorCount;
		bindingsDeducedInfo[i].nameOffset = XCheckedCastU16(namesBufferSize);

		namesBufferSize += XCheckedCastU16(binding.name.getLength() + 1); // +1 for zero terminator.
		descriptorCount += binding.descriptorCount;
	}

	// Check for duplicate names / hash collisions.
	// Using O(N^2) because I am lazy.
	for (uint16 i = 0; i < bindingCount; i++)
	{
		for (uint16 j = 0; j < bindingCount; j++)
		{
			if (i == j)
				continue;

			if (bindings[i].name == bindings[j].name)
			{
				XTODO("Proper error report")
				return nullptr;
			}

			// NOTE: IDK how many XSH bits I will use in the end but not less than 32.
			if (uint32(bindingsDeducedInfo[i].nameXSH) == uint32(bindingsDeducedInfo[j].nameXSH))
			{
				XTODO("Proper error report")
				return nullptr;
			}
		}
	}

	// Calculate source hash.
	uint32 sourceHash = 0;
	{
		XTODO("Calculate descriptor set layout source hash")
		// ...
	}

	BlobFormat::DescriptorSetLayoutBlobWriter serializedBlobWriter;
	serializedBlobWriter.initialize(bindingCount);
	const uint32 serializedBlobSize = serializedBlobWriter.getMemoryBlockSize();

	// Calculate required memory block size.
	uint32 memoryBlockSizeAccum = sizeof(DescriptorSetLayout);
	memoryBlockSizeAccum = alignUp<uint32>(memoryBlockSizeAccum, 16);

	const uint32 bindingsRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(DescriptorSetLayout::BindingDesc) * bindingCount;

	const uint32 namesBufferRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += namesBufferSize;
	memoryBlockSizeAccum = alignUp<uint32>(memoryBlockSizeAccum, 16);

	const uint32 serializedBlobRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += serializedBlobSize;

	// Allocate memory block.
	const uint32 memoryBlockSize = memoryBlockSizeAccum;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize); // Just in case.

	// Populate memory block.
	DescriptorSetLayout& resultObject = *(DescriptorSetLayout*)memoryBlock;
	DescriptorSetLayout::BindingDesc* internalBindings = (DescriptorSetLayout::BindingDesc*)(uintptr(memoryBlock) + bindingsRelativeOffset);
	char* namesBuffer = (char*)(uintptr(memoryBlock) + namesBufferRelativeOffset);
	void* serializedBlob = (void*)(uintptr(memoryBlock) + serializedBlobRelativeOffset);

	// Populate result object itself.
	{
		construct(resultObject);
		resultObject.bindings = internalBindings;
		resultObject.namesBuffer = StringViewASCII(namesBuffer, namesBufferSize);
		resultObject.serializedBlobData = serializedBlob;
		resultObject.serializedBlobSize = serializedBlobSize;
		resultObject.sourceHash = sourceHash;
		resultObject.descriptorCount = descriptorCount;
		resultObject.bindingCount = bindingCount;
	}

	// Populate internal bindings & names buffer.
	for (uint16 i = 0; i < bindingCount; i++)
	{
		const DescriptorSetBindingDesc& src = bindings[i];
		DescriptorSetLayout::BindingDesc& dst = internalBindings[i];

		dst.nameOffset = bindingsDeducedInfo[i].nameOffset;
		dst.nameLength = XCheckedCastU16(src.name.getLength());
		dst.descriptorOffset = bindingsDeducedInfo[i].descriptorOffset;
		dst.descriptorCount = src.descriptorCount;
		dst.descriptorType = src.descriptorType;

		memoryCopy(namesBuffer + dst.nameOffset, src.name.getData(), src.name.getLength());
		*(namesBuffer + dst.nameOffset + src.name.getLength()) = 0;
	}

	// Populate serialized blob.
	{
		serializedBlobWriter.setMemoryBlock(serializedBlob, serializedBlobSize);
		for (uint16 i = 0; i < bindingCount; i++)
		{
			const DescriptorSetBindingDesc& binding = bindings[i];
			BlobFormat::DescriptorSetBindingInfo blobBindingInfo = {};

			blobBindingInfo.nameXSH = bindingsDeducedInfo[i].nameXSH;
			blobBindingInfo.descriptorCount = binding.descriptorCount;
			blobBindingInfo.descriptorType = binding.descriptorType;

			serializedBlobWriter.writeBinding(i, blobBindingInfo);
		}
		serializedBlobWriter.finalize(sourceHash);
	}

	return DescriptorSetLayoutRef(&resultObject);
}


// PipelineLayout //////////////////////////////////////////////////////////////////////////////////

sint16 PipelineLayout::findBinding(StringViewASCII name) const
{
	for (uint16 i = 0; i < bindingCount; i++)
	{
		if (namesBuffer.getSubString(bindings[i].nameOffset, bindings[i].nameLength) == name)
			return i;
	}
	return -1;
}

PipelineBindingDesc PipelineLayout::getBindingDesc(uint16 bindingIndex) const
{
	XAssert(bindingIndex < bindingCount);
	const BindingDesc& internalDesc = bindings[bindingIndex];

	PipelineBindingDesc result = {};
	result.name = namesBuffer.getSubString(internalDesc.nameOffset, internalDesc.nameLength);
	result.type = internalDesc.type;
	return result;
}

uint32 PipelineLayout::getBindingBaseShaderRegister(uint16 bindingIndex) const
{
	XAssert(bindingIndex < bindingCount);
	return bindings[bindingIndex].baseShaderRegister;
}

uint32 PipelineLayout::getSerializedBlobChecksum() const
{
	return ((BlobFormat::Data::GenericBlobHeader*)getSerializedBlobData())->checksum;
}

PipelineLayoutRef PipelineLayout::Create(const PipelineBindingDesc* bindings, uint16 bindingCount)
{
	// NOTE: I am retarded, so I put all the data in a single heap allocation :autistic_face:

	XAssert(bindingCount <= MaxPipelineBindingCount);

	struct BindingDeducedInfo
	{
		uint64 nameXSH;
		uint32 baseShaderRegister;
		uint16 d3dRootParameterIndex;
		uint16 nameOffset;
	};
	BindingDeducedInfo bindingsDeducedInfo[MaxPipelineBindingCount] = {};

	// Generic checks for each binding.
	// Calculate bindings deduced info (name XSH, name offset)
	// Calculate names buffer length.
	uint32 namesBufferSize = 0;
	for (uint16 i = 0; i < bindingCount; i++)
	{
		const PipelineBindingDesc& binding = bindings[i];

		XAssert(!binding.name.isEmpty());
		XAssert(binding.name.getLength() <= MaxPipelineBindingNameLength);
		// TODO: Validate `binding.type` value.
		// TODO: Validate `binding.descriptorSetLayout` value.

		bindingsDeducedInfo[i].nameXSH = XSH::Compute(binding.name);
		bindingsDeducedInfo[i].nameOffset = XCheckedCastU16(namesBufferSize);

		namesBufferSize += XCheckedCastU16(binding.name.getLength() + 1); // +1 for zero terminator.
	}

	// Check for duplicate names / hash collisions.
	// Using O(N^2) because I am lazy.
	for (uint16 i = 0; i < bindingCount; i++)
	{
		for (uint16 j = 0; j < bindingCount; j++)
		{
			if (i == j)
				continue;

			if (bindings[i].name == bindings[j].name)
			{
				XTODO("Proper error report")
				return nullptr;
			}

			// NOTE: IDK how many XSH bits I will use in the end but not less than 32.
			if (uint32(bindingsDeducedInfo[i].nameXSH) == uint32(bindingsDeducedInfo[j].nameXSH))
			{
				XTODO("Proper error report")
				return nullptr;
			}
		}
	}

	// Calculate source hash.
	uint32 sourceHash = 0;
	{
		XTODO("Calculate pipeline layout source hash")
		// ...
	}

	// Compile D3D root signature.
	// Calculate bindings deduced info (D3D root parameter index, shader register)
	Microsoft::WRL::ComPtr<ID3DBlob> d3dRootSignature;
	Microsoft::WRL::ComPtr<ID3DBlob> d3dError;
	{
		InplaceArrayList<D3D12_ROOT_PARAMETER1, MaxPipelineBindingCount> d3dRootParams;
		InplaceArrayList<D3D12_DESCRIPTOR_RANGE1, 128> d3dDescriptorRanges; // TODO: Use ExpandableInplaceArrayList here.

		uint32 shaderRegisterCounter = StartShaderRegiserIndex;
		for (uint16 pipelineBindindIndex = 0; pipelineBindindIndex < bindingCount; pipelineBindindIndex++)
		{
			const PipelineBindingDesc& binding = bindings[pipelineBindindIndex];

			if (binding.type == PipelineBindingType::InplaceConstants)
			{
				// TODO: Implement it.
				XAssertUnreachableCode();
				continue;
			}

			const uint16 d3dRootParameterIndex = d3dRootParams.getSize();
			D3D12_ROOT_PARAMETER1& d3dRootParameter = d3dRootParams.emplaceBack();
			d3dRootParameter = {};

			bindingsDeducedInfo[pipelineBindindIndex].baseShaderRegister = shaderRegisterCounter;
			bindingsDeducedInfo[pipelineBindindIndex].d3dRootParameterIndex = d3dRootParameterIndex;

			if (binding.type == PipelineBindingType::ConstantBuffer)
			{
				d3dRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				d3dRootParameter.Descriptor.ShaderRegister = shaderRegisterCounter;
				d3dRootParameter.Descriptor.RegisterSpace = 0;
				d3dRootParameter.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
				shaderRegisterCounter++;
			}
			else if (binding.type == PipelineBindingType::ReadOnlyBuffer)
			{
				d3dRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
				d3dRootParameter.Descriptor.ShaderRegister = shaderRegisterCounter;
				d3dRootParameter.Descriptor.RegisterSpace = 0;
				d3dRootParameter.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
				shaderRegisterCounter++;
			}
			else if (binding.type == PipelineBindingType::ReadWriteBuffer)
			{
				d3dRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
				d3dRootParameter.Descriptor.ShaderRegister = shaderRegisterCounter;
				d3dRootParameter.Descriptor.RegisterSpace = 0;
				d3dRootParameter.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
				shaderRegisterCounter++;
			}
			else if (binding.type == PipelineBindingType::DescriptorSet)
			{
				const uint32 descriptorSetBaseShaderRegister = shaderRegisterCounter;
				const uint32 descriptorSetBaseRangeIndex = d3dDescriptorRanges.getSize();

				D3D12_DESCRIPTOR_RANGE1 d3dCurrentRange = {};
				bool currentRangeInitialized = false;

				for (uint16 i = 0; i < binding.descriptorSetLayout->getBindingCount(); i++)
				{
					const DescriptorSetBindingDesc descriptorSetBinding = binding.descriptorSetLayout->getBindingDesc(i);
					const uint32 descriptorSetBindingDescriptorOffset = binding.descriptorSetLayout->getBindingDescriptorOffset(i);

					const D3D12_DESCRIPTOR_RANGE_TYPE d3dRequiredRangeType =
						TranslateDescriptorTypeToD3D12DescriptorRangeType(descriptorSetBinding.descriptorType);

					if (!currentRangeInitialized || d3dCurrentRange.RangeType != d3dRequiredRangeType)
					{
						// Finalize previous range and start new one.
						if (currentRangeInitialized)
						{
							// Submit current range.
							d3dCurrentRange.NumDescriptors =
								descriptorSetBindingDescriptorOffset - d3dCurrentRange.OffsetInDescriptorsFromTableStart;

							XAssert(!d3dDescriptorRanges.isFull());
							d3dDescriptorRanges.pushBack(d3dCurrentRange);
						}

						d3dCurrentRange = {};
						d3dCurrentRange.RangeType = d3dRequiredRangeType;
						d3dCurrentRange.NumDescriptors = 0;
						d3dCurrentRange.BaseShaderRegister = descriptorSetBaseShaderRegister + descriptorSetBindingDescriptorOffset;
						d3dCurrentRange.RegisterSpace = 0;
						d3dCurrentRange.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
						d3dCurrentRange.OffsetInDescriptorsFromTableStart = descriptorSetBindingDescriptorOffset;
						currentRangeInitialized = true;
					}
				}

				// Submit final range.
				XAssert(currentRangeInitialized);
				{
					XAssert(binding.descriptorSetLayout->getDescriptorCount() > d3dCurrentRange.OffsetInDescriptorsFromTableStart);
					d3dCurrentRange.NumDescriptors =
						binding.descriptorSetLayout->getDescriptorCount() - d3dCurrentRange.OffsetInDescriptorsFromTableStart;

					XAssert(!d3dDescriptorRanges.isFull());
					d3dDescriptorRanges.pushBack(d3dCurrentRange);
				}

				d3dRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				d3dRootParameter.DescriptorTable.NumDescriptorRanges = d3dDescriptorRanges.getSize() - descriptorSetBaseRangeIndex;
				d3dRootParameter.DescriptorTable.pDescriptorRanges = &d3dDescriptorRanges[descriptorSetBaseRangeIndex];
				shaderRegisterCounter += binding.descriptorSetLayout->getDescriptorCount();
			}
			else
				XAssertUnreachableCode();

			d3dRootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // TODO: ...
		}

		// Compile root signature.
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3dRootSignatureDesc = {};
		d3dRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		d3dRootSignatureDesc.Desc_1_1.pParameters = d3dRootParams.getData();
		d3dRootSignatureDesc.Desc_1_1.NumParameters = d3dRootParams.getSize();
		d3dRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		// TODO: D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
		// TODO: D3D12_ROOT_SIGNATURE_FLAG_DENY_*_SHADER_ROOT_ACCESS

		D3D12SerializeVersionedRootSignature(&d3dRootSignatureDesc, &d3dRootSignature, &d3dError);
		// TODO: Check HRESULT.
	}

	const void* platformData = d3dRootSignature->GetBufferPointer();
	const uint32 platformDataSize = XCheckedCastU32(d3dRootSignature->GetBufferSize());

	BlobFormat::PipelineLayoutBlobWriter serializedBlobWriter;
	serializedBlobWriter.initialize(bindingCount, platformDataSize);
	const uint32 serializedBlobSize = serializedBlobWriter.getMemoryBlockSize();

	// Calculate required memory block size.
	uint32 memoryBlockSizeAccum = sizeof(PipelineLayout);
	memoryBlockSizeAccum = alignUp<uint32>(memoryBlockSizeAccum, 16);

	const uint32 bindingsRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(PipelineLayout::BindingDesc) * bindingCount;

	const uint32 namesBufferRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += namesBufferSize;
	memoryBlockSizeAccum = alignUp<uint32>(memoryBlockSizeAccum, 16);

	const uint32 serializedBlobRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += serializedBlobSize;

	// Allocate memory block.
	const uint32 memoryBlockSize = memoryBlockSizeAccum;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize); // Just in case.

	// Populate memory block.
	PipelineLayout& resultObject = *(PipelineLayout*)memoryBlock;
	PipelineLayout::BindingDesc* internalBindings = (PipelineLayout::BindingDesc*)(uintptr(memoryBlock) + bindingsRelativeOffset);
	char* namesBuffer = (char*)(uintptr(memoryBlock) + namesBufferRelativeOffset);
	void* serializedBlob = (void*)(uintptr(memoryBlock) + serializedBlobRelativeOffset);

	// Populate result object itself.
	{
		construct(resultObject);
		resultObject.bindings = internalBindings;
		resultObject.namesBuffer = StringViewASCII(namesBuffer, namesBufferSize);
		resultObject.serializedBlobData = serializedBlob;
		resultObject.serializedBlobSize = serializedBlobSize;
		resultObject.sourceHash = sourceHash;
		resultObject.bindingCount = bindingCount;
	}

	// Populate internal bindings & names buffer.
	for (uint16 i = 0; i < bindingCount; i++)
	{
		const PipelineBindingDesc& src = bindings[i];
		PipelineLayout::BindingDesc& dst = internalBindings[i];

		dst.nameOffset = bindingsDeducedInfo[i].nameOffset;
		dst.nameLength = XCheckedCastU16(src.name.getLength());
		dst.baseShaderRegister = bindingsDeducedInfo[i].baseShaderRegister;
		dst.type = src.type;

		if (src.type == GfxHAL::PipelineBindingType::DescriptorSet)
			resultObject.descriptorSetLayoutTable[i] = (DescriptorSetLayout*)src.descriptorSetLayout;

		memoryCopy(namesBuffer + dst.nameOffset, src.name.getData(), src.name.getLength());
		*(namesBuffer + dst.nameOffset + src.name.getLength()) = 0;
	}

	// Populate serialized blob.
	{
		serializedBlobWriter.setMemoryBlock(serializedBlob, serializedBlobSize);
		for (uint16 i = 0; i < bindingCount; i++)
		{
			const PipelineBindingDesc& binding = bindings[i];
			BlobFormat::PipelineBindingInfo blobBindingInfo = {};

			blobBindingInfo.nameXSH = bindingsDeducedInfo[i].nameXSH;
			blobBindingInfo.d3dRootParameterIndex = bindingsDeducedInfo[i].d3dRootParameterIndex;
			blobBindingInfo.type = binding.type;
			if (binding.type == GfxHAL::PipelineBindingType::DescriptorSet)
				blobBindingInfo.descriptorSetLayoutSourceHash = binding.descriptorSetLayout->getSourceHash();

			serializedBlobWriter.writeBinding(i, blobBindingInfo);
		}
		serializedBlobWriter.writePlatformData(platformData, platformDataSize);
		serializedBlobWriter.finalize(sourceHash);
	}

	return PipelineLayoutRef(&resultObject);
}


// Blob ////////////////////////////////////////////////////////////////////////////////////////////

uint32 Blob::getChecksum() const
{
	return ((BlobFormat::Data::GenericBlobHeader*)getData())->checksum;
}

BlobRef Blob::Create(uint32 size)
{
	const uint32 memoryBlockSize = sizeof(Blob) + size;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize); // Just in case.

	Blob& resultObject = *(Blob*)memoryBlock;
	construct(resultObject);
	resultObject.size = size;

	return BlobRef(&resultObject);
}


// ShaderCompiler //////////////////////////////////////////////////////////////////////////////////

static bool CompileShaderDXC(const PipelineLayout& pipelineLayout, const ShaderDesc& shader, ShaderType shaderType,
	ShaderCompilationArtifactsRef& artifacts, BlobRef& compiledBytecodeBlob)
{
	artifacts = nullptr;
	compiledBytecodeBlob = nullptr;

	// TODO: Preprocess source text.
	// ...

	// Patch source text.
	DynamicStringASCII patchedSource;
	{
		HLSLPatcher hlslPatcher(shader.sourceText, pipelineLayout);
		HLSLPatcher::Error hlslPatcherError = {};

		if (!hlslPatcher.execute(patchedSource, hlslPatcherError))
		{
			// TODO: Store XE HLSL patcher errors in artifacts.
			TextWriteFmtStdOut(shader.sourcePath, ":",
				hlslPatcherError.location.lineNumber, ":", hlslPatcherError.location.columnNumber,
				": xe-hlsl-patcher: ", hlslPatcherError.message);
			return false;
		}
	}

	// Build arguments list.
	wchar displayedShaderNameW[2048] = {};
	wchar entryPointNameW[128] = {};
	MultiByteToWideChar(CP_ACP, 0, shader.sourcePath.getData(), uint32(shader.sourcePath.getLength()), displayedShaderNameW, countof(displayedShaderNameW));
	MultiByteToWideChar(CP_ACP, 0, shader.entryPointName.getData(), uint32(shader.entryPointName.getLength()), entryPointNameW, countof(entryPointNameW));

	InplaceArrayList<LPCWSTR, 64> dxcArgsList;
	{
		dxcArgsList.pushBack(displayedShaderNameW);

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

		dxcArgsList.pushBack(L"-E");
		dxcArgsList.pushBack(entryPointNameW);

		//dxcArgsList.pushBack(L"-enable-16bit-types");
	}

	// Actual compilation.
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));

	DxcBuffer dxcSourceBuffer = {};
	dxcSourceBuffer.Ptr = patchedSource.getData();
	dxcSourceBuffer.Size = patchedSource.getLength();
	dxcSourceBuffer.Encoding = CP_UTF8;

	Microsoft::WRL::ComPtr<IDxcResult> dxcResult;
	const HRESULT hCompileResult = dxcCompiler->Compile(&dxcSourceBuffer,
		dxcArgsList.getData(), dxcArgsList.getSize(), nullptr, IID_PPV_ARGS(&dxcResult));
	if (FAILED(hCompileResult) || !dxcResult)
		return false;

	Microsoft::WRL::ComPtr<IDxcBlobUtf8> dxcErrorsBlob;
	dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&dxcErrorsBlob), nullptr);

	if (dxcErrorsBlob != nullptr && dxcErrorsBlob->GetStringLength() > 0)
	{
		// TODO: Store compiler output in artifacts.
		TextWriteFmtStdOut(dxcErrorsBlob->GetStringPointer());
	}

	HRESULT hCompileStatus = E_FAIL;
	dxcResult->GetStatus(&hCompileStatus);

	if (FAILED(hCompileStatus))
		return false;

	Microsoft::WRL::ComPtr<IDxcBlob> dxcBytecodeBlob = nullptr;
	dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxcBytecodeBlob), nullptr);
	XAssert(dxcBytecodeBlob != nullptr && dxcBytecodeBlob->GetBufferSize() > 0);

	// Compose compiled blob.
	BlobFormat::BytecodeBlobWriter blobWriter;
	blobWriter.initialize(uint32(dxcBytecodeBlob->GetBufferSize()));

	BlobRef blob = Blob::Create(blobWriter.getMemoryBlockSize());
	blobWriter.setMemoryBlock((void*)blob->getData(), blob->getSize());
	blobWriter.writeBytecode(dxcBytecodeBlob->GetBufferPointer(), uint32(dxcBytecodeBlob->GetBufferSize()));
	blobWriter.finalize(shaderType, pipelineLayout.getSourceHash());

	compiledBytecodeBlob = asRValue(blob);

	return true;
}

bool ShaderCompiler::CompileGraphicsPipeline(
	const PipelineLayout& pipelineLayout,
	const GraphicsPipelineShaders& shaders,
	const GraphicsPipelineSettings& settings,
	GraphicsPipelineCompilationArtifacts& artifacts,
	GraphicsPipelineCompiledBlobs& compiledBlobs)
{
	artifacts = {};
	compiledBlobs = {};

	const bool vsEnabled = !shaders.vs.sourcePath.isEmpty();
	const bool asEnabled = !shaders.as.sourcePath.isEmpty();
	const bool msEnabled = !shaders.ms.sourcePath.isEmpty();
	const bool psEnabled = !shaders.ps.sourcePath.isEmpty();

	if (vsEnabled) XAssert(!shaders.vs.sourceText.isEmpty());
	if (asEnabled) XAssert(!shaders.as.sourceText.isEmpty());
	if (msEnabled) XAssert(!shaders.ms.sourceText.isEmpty());
	if (psEnabled) XAssert(!shaders.ps.sourceText.isEmpty());

	// Verify enabled shader stages combination.
	if ((vsEnabled == msEnabled) || (!vsEnabled && !msEnabled) || (asEnabled && !msEnabled))
	{
		artifacts.pipelineCompilationOutput = "invalid shader stages combination";
		return false;
	}

	if (settings.vertexBindingCount > 0 && !vsEnabled)
	{
		artifacts.pipelineCompilationOutput = "vertex input but vertex shader disabled";
		return false;
	}

	if (settings.vertexBindingCount > MaxVertexBindingCount)
	{
		artifacts.pipelineCompilationOutput = "vertex bindings limit exceeded";
		return false;
	}

	// Compile shaders.
	BlobRef vsBlob = nullptr;
	BlobRef asBlob = nullptr;
	BlobRef msBlob = nullptr;
	BlobRef psBlob = nullptr;

	if (vsEnabled)
	{
		if (!CompileShaderDXC(pipelineLayout, shaders.vs, ShaderType::Vertex, artifacts.vs, vsBlob))
			return false;
	}
	if (asEnabled)
	{
		if (!CompileShaderDXC(pipelineLayout, shaders.as, ShaderType::Amplification, artifacts.as, asBlob))
			return false;
	}
	if (msEnabled)
	{
		if (!CompileShaderDXC(pipelineLayout, shaders.ms, ShaderType::Mesh, artifacts.ms, msBlob))
			return false;
	}
	if (psEnabled)
	{
		if (!CompileShaderDXC(pipelineLayout, shaders.ps, ShaderType::Pixel, artifacts.ps, psBlob))
			return false;
	}

	// Compose state blob.

	BlobFormat::GraphicsPipelineStateBlobWriter stateBlobWriter;
	stateBlobWriter.beginInitialization();

	if (vsEnabled) stateBlobWriter.registerBytecodeBlob(ShaderType::Vertex, vsBlob->getChecksum());
	if (asEnabled) stateBlobWriter.registerBytecodeBlob(ShaderType::Amplification, asBlob->getChecksum());
	if (msEnabled) stateBlobWriter.registerBytecodeBlob(ShaderType::Mesh, msBlob->getChecksum());
	if (psEnabled) stateBlobWriter.registerBytecodeBlob(ShaderType::Pixel, psBlob->getChecksum());

	// Validate and register render targets.
	{
		bool undefinedRenderTargetFound = false;
		for (TexelViewFormat renderTargetFormat : settings.renderTargetsFormats)
		{
			if (undefinedRenderTargetFound)
				XAssert(renderTargetFormat == TexelViewFormat::Undefined);
			else if (renderTargetFormat == TexelViewFormat::Undefined)
				undefinedRenderTargetFound = true;
			else
			{
				if (!ValidateTexelViewFormatValue(renderTargetFormat))
					return false;
				if (!GfxHAL::DoesTexelViewFormatSupportColorRenderTargetUsage(renderTargetFormat))
					return false;
				stateBlobWriter.addRenderTarget(renderTargetFormat);
			}
		}
	}

	// TODO: `ValidateDepthStencilFormatValue`.
	stateBlobWriter.setDepthStencilFormat(settings.depthStencilFormat);

	// Validate and register vertex input layout.
	for (uint8 i = 0; i < MaxVertexBufferCount; i++)
	{
		switch (settings.vertexBuffers[i].type)
		{
			case VertexBufferType::Undefined:	break;
			case VertexBufferType::PerVertex:	stateBlobWriter.enableVertexBuffer(i, false); break;
			case VertexBufferType::PerInstance:	stateBlobWriter.enableVertexBuffer(i, true);  break;
			default: return false;
		}
	}

	for (uint8 i = 0; i < settings.vertexBindingCount; i++)
	{
		const VertexBindingDesc& binding = settings.vertexBindings[i];

		if (binding.name.getLength() > stateBlobWriter.MaxVertexBindingNameLength)
			return false;
		if (!GfxHAL::DoesTexelViewFormatSupportVertexInputUsage(binding.format))
			return false;
		if (binding.offset >= MaxVertexBufferElementSize)
			return false;
		if (binding.bufferIndex >= MaxVertexBindingCount)
			return false;

		const VertexBufferDesc& buffer = settings.vertexBuffers[binding.bufferIndex];
		if (buffer.type != VertexBufferType::PerVertex && buffer.type != VertexBufferType::PerInstance)
			return false;

		stateBlobWriter.addVertexBinding(binding.name.getData(), binding.name.getLength(),
			binding.format, binding.offset, binding.bufferIndex);
	}

	stateBlobWriter.endInitialization();

	BlobRef stateBlob = Blob::Create(stateBlobWriter.getMemoryBlockSize());
	stateBlobWriter.finalizeToMemoryBlock((void*)stateBlob->getData(), stateBlob->getSize(), pipelineLayout.getSourceHash());

	compiledBlobs.stateBlob = asRValue(stateBlob);
	compiledBlobs.vsBytecodeBlob = asRValue(vsBlob);
	compiledBlobs.asBytecodeBlob = asRValue(asBlob);
	compiledBlobs.msBytecodeBlob = asRValue(msBlob);
	compiledBlobs.psBytecodeBlob = asRValue(psBlob);

	return true;
}

bool ShaderCompiler::CompileComputePipeline(
	const PipelineLayout& pipelineLayout,
	const ShaderDesc& computeShader,
	ShaderCompilationArtifactsRef& artifacts,
	BlobRef& compiledBlob)
{
	artifacts = nullptr;
	compiledBlob = nullptr;

	return CompileShaderDXC(pipelineLayout, computeShader, ShaderType::Compute, artifacts, compiledBlob);
}
