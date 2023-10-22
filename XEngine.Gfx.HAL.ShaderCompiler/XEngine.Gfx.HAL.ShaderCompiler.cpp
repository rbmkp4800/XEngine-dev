#include <d3d12.h>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <wrl/client.h>

#include <XLib.Containers.ArrayList.h>
#include <XLib.String.h>
#include <XLib.SystemHeapAllocator.h>
#include <XLib.System.Threading.Atomics.h>
#include <XLib.Text.h>

#include <XEngine.Gfx.HAL.BlobFormat.h>
#include <XEngine.XStringHash.h>

#include "XEngine.Gfx.HAL.ShaderCompiler.h"
#include "XEngine.Gfx.HAL.ShaderCompiler.HLSLPatcher.h"

using namespace XLib;
using namespace XEngine::Gfx::HAL;
using namespace XEngine::Gfx::HAL::ShaderCompiler;

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

static inline bool TranslateSamplerDescToD3D12StaticSamplerDesc(const SamplerDesc& srcSamplerDesc, D3D12_STATIC_SAMPLER_DESC& d3dResultSamplerDesc)
{
	XAssertNotImplemented();
	return false;
}


// DescriptorSetLayout /////////////////////////////////////////////////////////////////////////////

struct DescriptorSetLayout::InternalBindingDesc
{
	uint16 nameOffset;
	uint16 nameLength;
	uint32 descriptorOffset;
	uint16 descriptorCount;
	DescriptorType descriptorType;
};

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
	const InternalBindingDesc& internalDesc = bindings[bindingIndex];

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

DescriptorSetLayoutRef DescriptorSetLayout::Create(const DescriptorSetBindingDesc* bindings, uint16 bindingCount,
	GenericErrorMessage& errorMessage)
{
	// NOTE: I am retarded, so I put all the data in a single heap allocation :autistic_face:

	if (bindingCount > MaxDescriptorSetBindingCount)
	{
		errorMessage.text = "bindings limit exceeded";
		return nullptr;
	}

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

		const uint64 bindingNameXSH = XSH::Compute(binding.name);

		if (!ValidateDescriptorSetBindingName(binding.name))
		{
			errorMessage.text = "invalid binding name";
			return nullptr;
		}
		if (uint32(bindingNameXSH) == 0)
		{
			errorMessage.text = "descriptor set binding name XSH = 0. This is not allowed";
			return nullptr;
		}
		if (binding.descriptorCount != 1)
		{
			errorMessage.text = "only one descriptor per binding supported for now";
			return nullptr;
		}
		if (binding.descriptorType == DescriptorType::Undefined || binding.descriptorType >= DescriptorType::ValueCount)
		{
			errorMessage.text = "invalid descriptor type";
			return nullptr;
		}

		bindingsDeducedInfo[i].nameXSH = bindingNameXSH;
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
				errorMessage.text = "duplicate binding name";
				return nullptr;
			}

			if (uint32(bindingsDeducedInfo[i].nameXSH) == uint32(bindingsDeducedInfo[j].nameXSH))
			{
				errorMessage.text = "binding name XSH collision o_O";
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

	const uint32 internalBindingsRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(DescriptorSetLayout::InternalBindingDesc) * bindingCount;

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
	DescriptorSetLayout::InternalBindingDesc* internalBindings = (DescriptorSetLayout::InternalBindingDesc*)(uintptr(memoryBlock) + internalBindingsRelativeOffset);
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
		DescriptorSetLayout::InternalBindingDesc& dst = internalBindings[i];

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

struct PipelineLayout::InternalBindingDesc
{
	uint16 nameOffset;
	uint16 nameLength;
	uint32 baseShaderRegister;
	PipelineBindingType type;
};

struct PipelineLayout::InternalStaticSamplerDesc
{
	uint16 bindingNameOffset;
	uint16 bindingNameLength;
	uint32 baseShaderRegister;
};

sint16 PipelineLayout::findBinding(StringViewASCII name) const
{
	for (uint16 i = 0; i < bindingCount; i++)
	{
		if (namesBuffer.getSubString(bindings[i].nameOffset, bindings[i].nameLength) == name)
			return i;
	}
	return -1;
}

sint16 PipelineLayout::findStaticSampler(StringViewASCII bindingName) const
{
	for (uint16 i = 0; i < staticSamplerCount; i++)
	{
		if (namesBuffer.getSubString(staticSamplers[i].bindingNameOffset, staticSamplers[i].bindingNameLength) == bindingName)
			return i;
	}
	return -1;
}

PipelineBindingDesc PipelineLayout::getBindingDesc(uint16 bindingIndex) const
{
	XAssert(bindingIndex < bindingCount);
	const InternalBindingDesc& internalDesc = bindings[bindingIndex];

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

uint32 PipelineLayout::getStaticSamplerShaderRegister(uint16 staticSamplerIndex) const
{
	XAssert(staticSamplerIndex < staticSamplerCount);
	return staticSamplers[staticSamplerIndex].baseShaderRegister;
}

uint32 PipelineLayout::getSerializedBlobChecksum() const
{
	return ((BlobFormat::Data::GenericBlobHeader*)getSerializedBlobData())->checksum;
}

PipelineLayoutRef PipelineLayout::Create(const PipelineBindingDesc* bindings, uint16 bindingCount,
	const StaticSamplerDesc* staticSamplers, uint16 staticSamplerCount, GenericErrorMessage& errorMessage)
{
	// NOTE: I am retarded, so I put all the data in a single heap allocation :autistic_face:

	if (bindingCount > MaxPipelineBindingCount)
	{
		errorMessage.text = "bindings limit exceeded";
		return nullptr;
	}
	if (staticSamplerCount > MaxPipelineStaticSamplerCount)
	{
		errorMessage.text = "static samplers limit exceeded";
		return nullptr;
	}

	struct BindingDeducedInfo
	{
		uint64 nameXSH;
		uint32 baseShaderRegister;
		uint16 d3dRootParameterIndex;
		uint16 nameOffset;
	};
	BindingDeducedInfo bindingsDeducedInfo[MaxPipelineBindingCount] = {};

	struct StaticSamplerDeducedInfo
	{
		uint16 nameOffset;
	};
	StaticSamplerDeducedInfo staticSamplersDeducedInfo[MaxPipelineStaticSamplerCount] = {};

	D3D12_STATIC_SAMPLER_DESC d3dStaticSamplers[MaxPipelineStaticSamplerCount] = {};

	struct BindingNameInfo
	{
		StringViewASCII str;
		uint64 strXSH; // This should be zero for static samplers as we do not use XSH for these.
	};

	constexpr uint32 maxBindingNameCount = MaxPipelineBindingCount + MaxPipelineStaticSamplerCount;
	InplaceArrayList<BindingNameInfo, maxBindingNameCount> allBindingNames; // Real pipeline bindings + static samplers.

	// Generic checks for each binding.
	// Calculate bindings deduced info (name XSH, name offset)
	// Calculate names buffer length.
	uint32 namesBufferSize = 0;

	for (uint16 i = 0; i < bindingCount; i++)
	{
		const PipelineBindingDesc& binding = bindings[i];

		const uint64 bindingNameXSH = XSH::Compute(binding.name);

		if (!ValidatePipelineBindingName(binding.name))
		{
			errorMessage.text = "invalid binding name";
			return nullptr;
		}
		if (uint32(bindingNameXSH) == 0)
		{
			errorMessage.text = "pipeline binding name XSH = 0. This is not allowed";
			return nullptr;
		}
		if (binding.type == PipelineBindingType::Undefined || binding.type >= PipelineBindingType::ValueCount)
		{
			errorMessage.text = "invalid binding type";
			return nullptr;
		}
		XAssert(imply(binding.type == PipelineBindingType::DescriptorSet, binding.descriptorSetLayout));

		bindingsDeducedInfo[i].nameXSH = bindingNameXSH;
		bindingsDeducedInfo[i].nameOffset = XCheckedCastU16(namesBufferSize);

		namesBufferSize += XCheckedCastU16(binding.name.getLength() + 1); // +1 for zero terminator.

		allBindingNames.pushBack(BindingNameInfo { binding.name, bindingNameXSH });
	}

	for (uint16 i = 0; i < staticSamplerCount; i++)
	{
		const StaticSamplerDesc& sampler = staticSamplers[i];

		if (!ValidatePipelineBindingName(sampler.bindingName))
		{
			errorMessage.text = "invalid static sampler name";
			return nullptr;
		}
		if (!TranslateSamplerDescToD3D12StaticSamplerDesc(sampler.desc, d3dStaticSamplers[i]))
		{
			errorMessage.text = "invalid static sampler state";
			return nullptr;
		}

		staticSamplersDeducedInfo[i].nameOffset = XCheckedCastU16(namesBufferSize);

		namesBufferSize += XCheckedCastU16(sampler.bindingName.getLength() + 1); // +1 for zero terminator.

		allBindingNames.pushBack(BindingNameInfo { sampler.bindingName, 0 });
	}

	// Check for duplicate names / hash collisions.
	// Using O(N^2) because I am lazy.
	for (uint16 i = 0; i < allBindingNames.getSize(); i++)
	{
		for (uint16 j = 0; j < allBindingNames.getSize(); j++)
		{
			if (i == j)
				continue;

			if (allBindingNames[i].str == allBindingNames[j].str)
			{
				errorMessage.text = "duplicate binding name";
				return nullptr;
			}

			// We are checking XSH collisions only for real bindings, not for static samplers.
			// For static samplers XSH is set to zero.
			if (uint32(allBindingNames[i].strXSH) && uint32(allBindingNames[j].strXSH) &&
				uint32(allBindingNames[i].strXSH) == uint32(allBindingNames[j].strXSH))
			{
				errorMessage.text = "binding name XSH collision o_O";
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

	for (uint16 i = 0; i < staticSamplerCount; i++)
	{
		d3dStaticSamplers[i].ShaderRegister = shaderRegisterCounter;
		d3dStaticSamplers[i].RegisterSpace = shaderRegisterCounter;
		d3dStaticSamplers[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		shaderRegisterCounter++;
	}

	// Compile root signature.
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC d3dRootSignatureDesc = {};
	d3dRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	d3dRootSignatureDesc.Desc_1_1.NumParameters = d3dRootParams.getSize();
	d3dRootSignatureDesc.Desc_1_1.pParameters = d3dRootParams.getData();
	d3dRootSignatureDesc.Desc_1_1.NumStaticSamplers = staticSamplerCount;
	d3dRootSignatureDesc.Desc_1_1.pStaticSamplers = staticSamplerCount ? d3dStaticSamplers : nullptr;
	d3dRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// TODO: D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED, D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
	// TODO: D3D12_ROOT_SIGNATURE_FLAG_DENY_*_SHADER_ROOT_ACCESS
	// TODO: D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT

	const HRESULT hResult = D3D12SerializeVersionedRootSignature(&d3dRootSignatureDesc, &d3dRootSignature, &d3dError);
	if (FAILED(hResult))
	{
		errorMessage.text = "'D3D12SerializeVersionedRootSignature' failed";
		return nullptr;
	}

	const void* platformData = d3dRootSignature->GetBufferPointer();
	const uint32 platformDataSize = XCheckedCastU32(d3dRootSignature->GetBufferSize());

	BlobFormat::PipelineLayoutBlobWriter serializedBlobWriter;
	serializedBlobWriter.initialize(bindingCount, platformDataSize);
	const uint32 serializedBlobSize = serializedBlobWriter.getMemoryBlockSize();

	// Calculate required memory block size.
	uint32 memoryBlockSizeAccum = sizeof(PipelineLayout);
	memoryBlockSizeAccum = alignUp<uint32>(memoryBlockSizeAccum, 16);

	const uint32 internalBindingsRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(PipelineLayout::InternalBindingDesc) * bindingCount;

	const uint32 staticSamplersRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(PipelineLayout::InternalStaticSamplerDesc) * staticSamplerCount;

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
	PipelineLayout::InternalBindingDesc* internalBindings = (PipelineLayout::InternalBindingDesc*)(uintptr(memoryBlock) + internalBindingsRelativeOffset);
	PipelineLayout::InternalStaticSamplerDesc* internalStaticSamplers = (PipelineLayout::InternalStaticSamplerDesc*)(uintptr(memoryBlock) + staticSamplersRelativeOffset);
	char* namesBuffer = (char*)(uintptr(memoryBlock) + namesBufferRelativeOffset);
	void* serializedBlob = (void*)(uintptr(memoryBlock) + serializedBlobRelativeOffset);

	// Populate result object itself.
	{
		construct(resultObject);
		resultObject.bindings = internalBindings;
		resultObject.staticSamplers = internalStaticSamplers;
		resultObject.namesBuffer = StringViewASCII(namesBuffer, namesBufferSize);
		resultObject.serializedBlobData = serializedBlob;
		resultObject.serializedBlobSize = serializedBlobSize;
		resultObject.sourceHash = sourceHash;
		resultObject.bindingCount = bindingCount;
		resultObject.staticSamplerCount = staticSamplerCount;
	}

	// Populate internal bindings & names buffer.
	for (uint16 i = 0; i < bindingCount; i++)
	{
		const PipelineBindingDesc& src = bindings[i];
		PipelineLayout::InternalBindingDesc& dst = internalBindings[i];

		dst.nameOffset = bindingsDeducedInfo[i].nameOffset;
		dst.nameLength = XCheckedCastU16(src.name.getLength());
		dst.baseShaderRegister = bindingsDeducedInfo[i].baseShaderRegister;
		dst.type = src.type;

		if (src.type == PipelineBindingType::DescriptorSet)
			resultObject.descriptorSetLayoutTable[i] = (DescriptorSetLayout*)src.descriptorSetLayout;

		memoryCopy(namesBuffer + dst.nameOffset, src.name.getData(), src.name.getLength());
		*(namesBuffer + dst.nameOffset + src.name.getLength()) = 0;
	}

	// Populate internal static samplers & names buffer.
	for (uint16 i = 0; i < staticSamplerCount; i++)
	{
		const StaticSamplerDesc& src = staticSamplers[i];
		PipelineLayout::InternalStaticSamplerDesc& dst = internalStaticSamplers[i];

		dst.bindingNameOffset = staticSamplersDeducedInfo[i].nameOffset;
		dst.bindingNameLength = XCheckedCastU16(src.bindingName.getLength());
		dst.baseShaderRegister = d3dStaticSamplers[i].ShaderRegister;

		memoryCopy(namesBuffer + dst.bindingNameOffset, src.bindingName.getData(), src.bindingName.getLength());
		*(namesBuffer + dst.bindingNameOffset + src.bindingName.getLength()) = 0;
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
			if (binding.type == PipelineBindingType::DescriptorSet)
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

static bool ValidateGenericBindingName(StringViewASCII name)
{
	if (name.isEmpty())
		return false;
	if (!Char::IsUpper(name[0]) && name[0] != '_')
		return false;

	for (uintptr i = 1; i < name.getLength(); i++)
	{
		const char c = name[i];
		if (!Char::IsUpper(c) && !Char::IsDigit(c) && c != '_')
			return false;
	}
	return true;
}

bool ShaderCompiler::ValidateVertexBindingName(StringViewASCII name)
{
	return ValidateGenericBindingName(name);
}

bool ShaderCompiler::ValidateDescriptorSetBindingName(StringViewASCII name)
{
	return ValidateGenericBindingName(name) && name.getLength() <= MaxDescriptorSetBindingNameLength;
}

bool ShaderCompiler::ValidatePipelineBindingName(StringViewASCII name)
{
	return ValidateGenericBindingName(name) && name.getLength() <= MaxPipelineBindingNameLength;
}

bool ShaderCompiler::ValidateShaderEntryPointName(StringViewASCII name)
{
	if (name.isEmpty())
		return false;
	if (!Char::IsLetter(name[0]) && name[0] != '_')
		return false;

	for (uintptr i = 1; i < name.getLength(); i++)
	{
		const char c = name[i];
		if (!Char::IsLetterOrDigit(c) && c != '_')
			return false;
	}
	return true;
}

static bool CompileShaderDXC(const PipelineLayout& pipelineLayout, const ShaderDesc& shader, ShaderType shaderType,
	ShaderCompilationArtifactsRef& artifacts, BlobRef& compiledBytecodeBlob)
{
	artifacts = nullptr;
	compiledBytecodeBlob = nullptr;

	// TODO: Preprocess source text.
	// ...

	// Patch source text.
	DynamicStringASCII patchedSource;
	HLSLPatcher::Error hlslPatcherError = {};
	if (!HLSLPatcher::Patch(shader.sourceText, pipelineLayout, patchedSource, hlslPatcherError))
	{
		// TODO: Store XE HLSL patcher errors in artifacts.
		TextWriteFmtStdOut(shader.sourcePath, ":",
			hlslPatcherError.location.lineNumber, ":", hlslPatcherError.location.columnNumber,
			": xe-hlsl-patcher: ", hlslPatcherError.message);
		return false;
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

		dxcArgsList.pushBack(L"-Zpr"); // Row-major.
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
	GraphicsPipelineCompiledBlobs& compiledBlobs,
	GenericErrorMessage& errorMessage)
{
	artifacts = {};
	compiledBlobs = {};

	const bool vsEnabled = !shaders.vs.sourcePath.isEmpty();
	const bool asEnabled = !shaders.as.sourcePath.isEmpty();
	const bool msEnabled = !shaders.ms.sourcePath.isEmpty();
	const bool psEnabled = !shaders.ps.sourcePath.isEmpty();

	// Verify enabled shader stages combination.
	if ((vsEnabled == msEnabled) || (!vsEnabled && !msEnabled) || (asEnabled && !msEnabled))
	{
		errorMessage.text = "invalid enabled shader stages combination";
		return false;
	}

	if (settings.vertexBindingCount > 0 && !vsEnabled)
	{
		errorMessage.text = "vertex input enabled but vertex shader disabled";
		return false;
	}

	if (settings.vertexBindingCount > MaxVertexBindingCount)
	{
		errorMessage.text = "vertex input: bindings limit exceeded";
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
		{
			errorMessage.text = "VS compilation failed";
			return false;
		}
	}
	if (asEnabled)
	{
		if (!CompileShaderDXC(pipelineLayout, shaders.as, ShaderType::Amplification, artifacts.as, asBlob))
		{
			errorMessage.text = "AS compilation failed";
			return false;
		}
	}
	if (msEnabled)
	{
		if (!CompileShaderDXC(pipelineLayout, shaders.ms, ShaderType::Mesh, artifacts.ms, msBlob))
		{
			errorMessage.text = "MS compilation failed";
			return false;
		}
	}
	if (psEnabled)
	{
		if (!CompileShaderDXC(pipelineLayout, shaders.ps, ShaderType::Pixel, artifacts.ps, psBlob))
		{
			errorMessage.text = "PS compilation failed";
			return false;
		}
	}

	// Compose state blob.

	BlobFormat::GraphicsPipelineStateBlobWriter stateBlobWriter;
	stateBlobWriter.beginInitialization();

	if (vsEnabled) stateBlobWriter.registerBytecodeBlob(ShaderType::Vertex, vsBlob->getChecksum());
	if (asEnabled) stateBlobWriter.registerBytecodeBlob(ShaderType::Amplification, asBlob->getChecksum());
	if (msEnabled) stateBlobWriter.registerBytecodeBlob(ShaderType::Mesh, msBlob->getChecksum());
	if (psEnabled) stateBlobWriter.registerBytecodeBlob(ShaderType::Pixel, psBlob->getChecksum());

	// Validate and register render targets.
	for (uint8 i = 0; i < countof(settings.colorRTFormats); i++)
	{
		if (settings.colorRTFormats[i] == TexelViewFormat::Undefined)
			continue;

		if (!TexelViewFormatUtils::IsValid(settings.colorRTFormats[i]))
		{
			errorMessage.text = "color render targets: invalid format";
			return false;
		}
		if (!TexelViewFormatUtils::SupportsColorRTUsage(settings.colorRTFormats[i]))
		{
			errorMessage.text = "color render targets: format does not support render target usage";
			return false;
		}
		stateBlobWriter.setColorRTFormat(i, settings.colorRTFormats[i]);
	}

	// TODO: `ValidateDepthStencilFormatValue`.
	stateBlobWriter.setDepthStencilRTFormat(settings.depthStencilRTFormat);

	// Validate and register vertex input layout.
	for (uint8 i = 0; i < MaxVertexBufferCount; i++)
	{
		switch (settings.vertexBuffers[i].stepRate)
		{
			case VertexBufferStepRate::Undefined:	break;
			case VertexBufferStepRate::PerVertex:	stateBlobWriter.enableVertexBuffer(i, false); break;
			case VertexBufferStepRate::PerInstance:	stateBlobWriter.enableVertexBuffer(i, true);  break;
			default:
				errorMessage.text = "vertex input: invalid buffer step rate";
				return false;
		}
	}

	for (uint8 i = 0; i < settings.vertexBindingCount; i++)
	{
		const VertexBindingDesc& binding = settings.vertexBindings[i];

		uint32 bindingNameLength = 0;
		while (bindingNameLength < countof(binding.nameCStr) && binding.nameCStr[bindingNameLength])
			bindingNameLength++;

		if (bindingNameLength == countof(binding.nameCStr)) // No zero terminator
		{
			errorMessage.text = "vertex input: binding name is too long (buffer overrun)";
			return false;
		}
		if (!ValidateVertexBindingName(StringViewASCII(binding.nameCStr, bindingNameLength)))
		{
			errorMessage.text = "vertex input: invalid binding name";
			return false;
		}
		if (!TexelViewFormatUtils::SupportsVertexInputUsage(binding.format))
		{
			errorMessage.text = "vertex input: binding format does not support vertex input usage";
			return false;
		}
		if (binding.offset + TexelViewFormatUtils::GetByteSize(binding.format) > MaxVertexBufferElementSize)
		{
			errorMessage.text = "vertex input: buffer element size limit exceeded";
			return false;
		}
		if (binding.bufferIndex >= MaxVertexBufferCount)
		{
			errorMessage.text = "vertex input: invalid vertex buffer index";
			return false;
		}

		const VertexBufferDesc& buffer = settings.vertexBuffers[binding.bufferIndex];
		if (buffer.stepRate != VertexBufferStepRate::PerVertex && buffer.stepRate != VertexBufferStepRate::PerInstance)
		{
			errorMessage.text = "vertex input: invalid vertex buffer index";
			return false;
		}

		BlobFormat::VertexBindingInfo blobBindingInfo = {};
		static_assert(sizeof(blobBindingInfo.nameCStr) >= sizeof(binding.nameCStr));
		memoryCopy(blobBindingInfo.nameCStr, binding.nameCStr, bindingNameLength);
		blobBindingInfo.offset = binding.offset;
		blobBindingInfo.format = binding.format;
		blobBindingInfo.bufferIndex = binding.bufferIndex;

		stateBlobWriter.addVertexBinding(blobBindingInfo);
	}

	XTODO("Sort vertex input bindings via buffer index and offset");

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
