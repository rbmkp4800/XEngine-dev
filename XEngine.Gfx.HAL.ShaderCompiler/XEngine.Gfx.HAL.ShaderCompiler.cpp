#include <d3d12.h> // TODO: This should be removed (including reference to Microsoft.Direct3D.D3D12 include folder)
#include <d3d12shader.h>
#include <dxcapi.h>
#include <wrl/client.h>

#include <XLib.Allocation.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Fmt.h>
#include <XLib.String.h>

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

static inline bool TranslateSamplerFilterModeAndReductionModeToD3D12Filter(SamplerFilterMode filterMode, SamplerReductionMode reductionMode, D3D12_FILTER& d3dResultFilter)
{
	if (reductionMode == SamplerReductionMode::WeightedAverage)
	{
		switch (filterMode)
		{
			case SamplerFilterMode::MinPnt_MagPnt_MipPnt:	d3dResultFilter = D3D12_FILTER_MIN_MAG_MIP_POINT;				break;
			case SamplerFilterMode::MinPnt_MagPnt_MipLin:	d3dResultFilter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;		break;
			case SamplerFilterMode::MinPnt_MagLin_MipPnt:	d3dResultFilter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;	break;
			case SamplerFilterMode::MinPnt_MagLin_MipLin:	d3dResultFilter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;		break;
			case SamplerFilterMode::MinLin_MagPnt_MipPnt:	d3dResultFilter = D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;		break;
			case SamplerFilterMode::MinLin_MagPnt_MipLin:	d3dResultFilter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;	break;
			case SamplerFilterMode::MinLin_MagLin_MipPnt:	d3dResultFilter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;		break;
			case SamplerFilterMode::MinLin_MagLin_MipLin:	d3dResultFilter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;				break;
			case SamplerFilterMode::Anisotropic:			d3dResultFilter = D3D12_FILTER_ANISOTROPIC;						break;
			default:
				return false;
		}
	}
	/*else if (reductionMode == SamplerReductionMode::WeightedAverageOfComparisonResult)
	{
		switch (filterMode)
		{
			case SamplerFilterMode::MinPnt_MagPnt_MipPnt:	d3dResultFilter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;				break;
			case SamplerFilterMode::MinPnt_MagPnt_MipLin:	d3dResultFilter = D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;			break;
			case SamplerFilterMode::MinPnt_MagLin_MipPnt:	d3dResultFilter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;	break;
			case SamplerFilterMode::MinPnt_MagLin_MipLin:	d3dResultFilter = D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;			break;
			case SamplerFilterMode::MinLin_MagPnt_MipPnt:	d3dResultFilter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;			break;
			case SamplerFilterMode::MinLin_MagPnt_MipLin:	d3dResultFilter = D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;	break;
			case SamplerFilterMode::MinLin_MagLin_MipPnt:	d3dResultFilter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;			break;
			case SamplerFilterMode::MinLin_MagLin_MipLin:	d3dResultFilter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;				break;
			case SamplerFilterMode::Anisotropic:			d3dResultFilter = D3D12_FILTER_COMPARISON_ANISOTROPIC;						break;
			default:
				return false;
		}
	}*/
	else if (reductionMode == SamplerReductionMode::WeightedAverageOfComparisonResult)
	{
		switch (filterMode)
		{
			case SamplerFilterMode::MinPnt_MagPnt_MipPnt:	d3dResultFilter = D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT;				break;
			case SamplerFilterMode::MinPnt_MagPnt_MipLin:	d3dResultFilter = D3D12_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR;		break;
			case SamplerFilterMode::MinPnt_MagLin_MipPnt:	d3dResultFilter = D3D12_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;	break;
			case SamplerFilterMode::MinPnt_MagLin_MipLin:	d3dResultFilter = D3D12_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR;		break;
			case SamplerFilterMode::MinLin_MagPnt_MipPnt:	d3dResultFilter = D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT;		break;
			case SamplerFilterMode::MinLin_MagPnt_MipLin:	d3dResultFilter = D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;	break;
			case SamplerFilterMode::MinLin_MagLin_MipPnt:	d3dResultFilter = D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT;		break;
			case SamplerFilterMode::MinLin_MagLin_MipLin:	d3dResultFilter = D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR;				break;
			case SamplerFilterMode::Anisotropic:			d3dResultFilter = D3D12_FILTER_MINIMUM_ANISOTROPIC;						break;
			default:
				return false;
		}
	}
	else if (reductionMode == SamplerReductionMode::WeightedAverageOfComparisonResult)
	{
		switch (filterMode)
		{
			case SamplerFilterMode::MinPnt_MagPnt_MipPnt:	d3dResultFilter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT;				break;
			case SamplerFilterMode::MinPnt_MagPnt_MipLin:	d3dResultFilter = D3D12_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR;		break;
			case SamplerFilterMode::MinPnt_MagLin_MipPnt:	d3dResultFilter = D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT;	break;
			case SamplerFilterMode::MinPnt_MagLin_MipLin:	d3dResultFilter = D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR;		break;
			case SamplerFilterMode::MinLin_MagPnt_MipPnt:	d3dResultFilter = D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT;		break;
			case SamplerFilterMode::MinLin_MagPnt_MipLin:	d3dResultFilter = D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR;	break;
			case SamplerFilterMode::MinLin_MagLin_MipPnt:	d3dResultFilter = D3D12_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT;		break;
			case SamplerFilterMode::MinLin_MagLin_MipLin:	d3dResultFilter = D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR;				break;
			case SamplerFilterMode::Anisotropic:			d3dResultFilter = D3D12_FILTER_MAXIMUM_ANISOTROPIC;						break;
			default:
				return false;
		}
	}
	else
		return false;

	return true;
}

static inline bool TranslateSamplerAddressModeToD3D12TextureAddressMode(SamplerAddressMode addressMode, D3D12_TEXTURE_ADDRESS_MODE& d3dResultAddressMode)
{
	switch (addressMode)
	{
		case SamplerAddressMode::Wrap:			d3dResultAddressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		return true;
		case SamplerAddressMode::Mirror:		d3dResultAddressMode = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;	return true;
		case SamplerAddressMode::Clamp:			d3dResultAddressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;	return true;
		case SamplerAddressMode::BorderZero:	d3dResultAddressMode = D3D12_TEXTURE_ADDRESS_MODE_BORDER;	return true;
	}
	return false;
}

static inline bool TranslateSamplerDescToD3D12StaticSamplerDesc(const SamplerDesc& samplerDesc, D3D12_STATIC_SAMPLER_DESC& d3dResultSamplerDesc)
{
	d3dResultSamplerDesc = {};
	
	if (!TranslateSamplerFilterModeAndReductionModeToD3D12Filter(samplerDesc.filterMode, samplerDesc.reductionMode, d3dResultSamplerDesc.Filter))
		return false;
	if (!TranslateSamplerAddressModeToD3D12TextureAddressMode(samplerDesc.addressModeU, d3dResultSamplerDesc.AddressU))
		return false;
	if (!TranslateSamplerAddressModeToD3D12TextureAddressMode(samplerDesc.addressModeV, d3dResultSamplerDesc.AddressV))
		return false;
	if (!TranslateSamplerAddressModeToD3D12TextureAddressMode(samplerDesc.addressModeW, d3dResultSamplerDesc.AddressW))
		return false;

	d3dResultSamplerDesc.MipLODBias = samplerDesc.lodBias;
	d3dResultSamplerDesc.MaxAnisotropy = samplerDesc.maxAnisotropy;
	//d3dResultSamplerDesc.ComparisonFunc = ...; // TODO: ...
	d3dResultSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	d3dResultSamplerDesc.MinLOD = samplerDesc.lodMin;
	d3dResultSamplerDesc.MaxLOD = samplerDesc.lodMax;

	return true;
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

	BlobFormat::DescriptorSetLayoutBlobInfo blobInfo = {};
	blobInfo.bindingCount = bindingCount;
	blobInfo.sourceHash = sourceHash;

	BlobFormat::DescriptorSetLayoutBlobWriter blobWriter;
	blobWriter.setup(blobInfo);
	const uint32 blobSize = blobWriter.getBlobSize();

	// Calculate required memory block size.
	uint32 memoryBlockSizeAccum = sizeof(DescriptorSetLayout);
	memoryBlockSizeAccum = alignUp<uint32>(memoryBlockSizeAccum, 16);

	const uint32 internalBindingsRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(DescriptorSetLayout::InternalBindingDesc) * bindingCount;

	const uint32 namesBufferRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += namesBufferSize;
	memoryBlockSizeAccum = alignUp<uint32>(memoryBlockSizeAccum, 16);

	const uint32 blobDataRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += blobSize;

	// Allocate memory block.
	const uint32 memoryBlockSize = memoryBlockSizeAccum;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize); // Just in case.

	// Populate memory block.
	DescriptorSetLayout& resultObject = *(DescriptorSetLayout*)memoryBlock;
	DescriptorSetLayout::InternalBindingDesc* internalBindings = (DescriptorSetLayout::InternalBindingDesc*)(uintptr(memoryBlock) + internalBindingsRelativeOffset);
	char* namesBuffer = (char*)(uintptr(memoryBlock) + namesBufferRelativeOffset);
	void* blobData = (void*)(uintptr(memoryBlock) + blobDataRelativeOffset);

	// Populate result object itself.
	{
		construct(resultObject);
		resultObject.bindings = internalBindings;
		resultObject.namesBuffer = StringViewASCII(namesBuffer, namesBufferSize);
		resultObject.blobData = blobData;
		resultObject.blobSize = blobSize;
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

	// Populate blob.
	{
		blobWriter.setBlobMemory(blobData, blobSize);
		for (uint16 i = 0; i < bindingCount; i++)
		{
			const DescriptorSetBindingDesc& binding = bindings[i];
			BlobFormat::DescriptorSetBindingInfo blobBindingInfo = {};

			blobBindingInfo.nameXSH = bindingsDeducedInfo[i].nameXSH;
			blobBindingInfo.descriptorCount = binding.descriptorCount;
			blobBindingInfo.descriptorType = binding.descriptorType;

			blobWriter.writeBindingInfo(i, blobBindingInfo);
		}
		blobWriter.finalize();
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
		d3dStaticSamplers[i].RegisterSpace = 0;
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
	d3dRootSignatureDesc.Desc_1_1.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;

	// TODO: D3D12_ROOT_SIGNATURE_FLAG_DENY_*_SHADER_ROOT_ACCESS

	const HRESULT hResult = D3D12SerializeVersionedRootSignature(&d3dRootSignatureDesc, &d3dRootSignature, &d3dError);
	if (FAILED(hResult))
	{
		errorMessage.text = "D3D12SerializeVersionedRootSignature failed";

		if (d3dError && d3dError->GetBufferSize() > 0)
			FmtPrintStdOut("D3D12SerializeVersionedRootSignature error: ", (char*)d3dError->GetBufferPointer(), "\n");

		return nullptr;
	}

	if (d3dError && d3dError->GetBufferSize() > 0)
	{
		// TODO: We might want to forward warnings.
		FmtPrintStdOut("#### `D3D12SerializeVersionedRootSignature` wants to say something!!!\n");
	}

	const void* platformData = d3dRootSignature->GetBufferPointer();
	const uint32 platformDataSize = XCheckedCastU32(d3dRootSignature->GetBufferSize());

	BlobFormat::PipelineLayoutBlobInfo blobInfo = {};
	blobInfo.bindingCount = bindingCount;
	blobInfo.platformDataSize = platformDataSize;
	blobInfo.sourceHash = sourceHash;

	BlobFormat::PipelineLayoutBlobWriter blobWriter;
	blobWriter.setup(blobInfo);
	const uint32 blobSize = blobWriter.getBlobSize();

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

	const uint32 blobDataRelativeOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += blobSize;

	// Allocate memory block.
	const uint32 memoryBlockSize = memoryBlockSizeAccum;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize); // Just in case.

	// Populate memory block.
	PipelineLayout& resultObject = *(PipelineLayout*)memoryBlock;
	PipelineLayout::InternalBindingDesc* internalBindings = (PipelineLayout::InternalBindingDesc*)(uintptr(memoryBlock) + internalBindingsRelativeOffset);
	PipelineLayout::InternalStaticSamplerDesc* internalStaticSamplers = (PipelineLayout::InternalStaticSamplerDesc*)(uintptr(memoryBlock) + staticSamplersRelativeOffset);
	char* namesBuffer = (char*)(uintptr(memoryBlock) + namesBufferRelativeOffset);
	void* blobData = (void*)(uintptr(memoryBlock) + blobDataRelativeOffset);

	// Populate result object itself.
	{
		construct(resultObject);
		resultObject.bindings = internalBindings;
		resultObject.staticSamplers = internalStaticSamplers;
		resultObject.namesBuffer = StringViewASCII(namesBuffer, namesBufferSize);
		resultObject.blobData = blobData;
		resultObject.blobSize = blobSize;
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

	// Populate blob.
	{
		blobWriter.setBlobMemory(blobData, blobSize);
		for (uint16 i = 0; i < bindingCount; i++)
		{
			const PipelineBindingDesc& binding = bindings[i];
			BlobFormat::PipelineBindingInfo blobBindingInfo = {};

			blobBindingInfo.nameXSH = bindingsDeducedInfo[i].nameXSH;
			blobBindingInfo.d3dRootParameterIndex = bindingsDeducedInfo[i].d3dRootParameterIndex;
			blobBindingInfo.type = binding.type;
			if (binding.type == PipelineBindingType::DescriptorSet)
				blobBindingInfo.descriptorSetLayoutSourceHash = binding.descriptorSetLayout->getSourceHash();

			blobWriter.writeBindingInfo(i, blobBindingInfo);
		}
		blobWriter.writePlatformData(platformData, platformDataSize);
		blobWriter.finalize();
	}

	return PipelineLayoutRef(&resultObject);
}


// Blob ////////////////////////////////////////////////////////////////////////////////////////////

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


// ShaderCompilationResult /////////////////////////////////////////////////////////////////////////

ShaderCompilationResultRef ShaderCompilationResult::Compose(const ComposerSource& source)
{
	uintptr memoryBlockSizeAccum = 0;
	memoryBlockSizeAccum += sizeof(ShaderCompilationResult);

	const uintptr preprocessingOuputStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += source.preprocessingOuputStr.getLength() + 1;

	const uintptr compilationOutputStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += source.compilationOutputStr.getLength() + 1;

	const uintptr memoryBlockSize = memoryBlockSizeAccum;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize);

	ShaderCompilationResult& resultObject = *(ShaderCompilationResult*)memoryBlock;
	construct(resultObject);

	memoryCopy((char*)memoryBlock + preprocessingOuputStrOffset, source.preprocessingOuputStr.getData(), source.preprocessingOuputStr.getLength());
	memoryCopy((char*)memoryBlock + compilationOutputStrOffset, source.compilationOutputStr.getData(), source.compilationOutputStr.getLength());

	resultObject.preprocessingOuputStr = StringViewASCII((char*)memoryBlock + preprocessingOuputStrOffset, source.preprocessingOuputStr.getLength());
	resultObject.compilationOutputStr = StringViewASCII((char*)memoryBlock + compilationOutputStrOffset, source.compilationOutputStr.getLength());

	resultObject.preprocessedSourceBlob = (Blob*)source.preprocessedSourceBlob;
	resultObject.bytecodeBlob = (Blob*)source.bytecodeBlob;
	resultObject.disassemblyBlob = (Blob*)source.disassemblyBlob;
	resultObject.d3dPDBBlob = (Blob*)source.d3dPDBBlob;

	resultObject.status = source.status;

	return ShaderCompilationResultRef(&resultObject);
}


// ShaderCompiler //////////////////////////////////////////////////////////////////////////////////

static bool ValidateGenericBindingName(StringViewASCII name)
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

ShaderCompilationResultRef ShaderCompiler::CompileShader(XLib::StringViewASCII mainSourceFilename,
	const ShaderCompilationArgs& args, const PipelineLayout& pipelineLayout,
	SourceResolverFunc sourceResolverFunc, void* sourceResolverContext)
{
	wchar mainSourceFilenameW[2048] = {};
	wchar entryPointNameW[128] = {};
	MultiByteToWideChar(CP_ACP, 0, mainSourceFilename.getData(), uint32(mainSourceFilename.getLength()), mainSourceFilenameW, countof(mainSourceFilenameW));
	MultiByteToWideChar(CP_ACP, 0, args.entryPointName.getData(), uint32(args.entryPointName.getLength()), entryPointNameW, countof(entryPointNameW));

	static Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils;
	if (!dxcUtils)
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));

	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler;
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));

	// Preprocess source via DXC ///////////////////////////////////////////////////////////////////
	HRESULT preprocessingStatusHResult = E_FAIL;
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> dxcPreprocessingErrorsBlob;
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> dxcPreprocessedSourceBlob;
	{
		InplaceArrayList<LPCWSTR, 4> dxcArgsList;
		dxcArgsList.pushBack(mainSourceFilenameW);
		dxcArgsList.pushBack(L"-P");

		DxcBuffer dxcMainSourceBuffer = {};
		{
			SourceResolutionResult mainSourceResolutionResult = sourceResolverFunc(sourceResolverContext, mainSourceFilename);
			if (!mainSourceResolutionResult.resolved)
			{
				XTODO(__FUNCTION__ ": Handle case when main source file is not resolved");
				XAssertUnreachableCode();
				return nullptr;
			}
			dxcMainSourceBuffer.Ptr = mainSourceResolutionResult.text.getData();
			dxcMainSourceBuffer.Size = mainSourceResolutionResult.text.getLength();
			dxcMainSourceBuffer.Encoding = CP_UTF8;
		}

		struct CustomDxcIncludeHandler : public IDxcIncludeHandler
		{
			SourceResolverFunc sourceResolverFunc = nullptr;
			void* sourceResolverContext = nullptr;

			IDxcUtils* dxcUtils = nullptr;
			UINT refCount = 0;

			virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override { XAssertUnreachableCode(); return 0; }
			virtual ULONG __stdcall AddRef() override { refCount++; return refCount; }
			virtual ULONG __stdcall Release() override { refCount--; return refCount; }

			virtual HRESULT __stdcall LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override
			{
				InplaceStringASCIIx1024 filename;
				for (int i = 0; pFilename[i]; i++)
					filename.append(char(pFilename[i]));

				SourceResolutionResult resolutionResult = sourceResolverFunc(sourceResolverContext, filename);
				if (!resolutionResult.resolved)
				{
					*ppIncludeSource = nullptr;
					return E_FAIL;
				}

				IDxcBlobEncoding* dxcBlob = nullptr;
				dxcUtils->CreateBlob(resolutionResult.text.getData(), XCheckedCastU32(resolutionResult.text.getLength()), CP_UTF8, &dxcBlob);
				*ppIncludeSource = dxcBlob;
				return S_OK;
			}
		};

		CustomDxcIncludeHandler includeHandler;
		includeHandler.sourceResolverFunc = sourceResolverFunc;
		includeHandler.sourceResolverContext = sourceResolverContext;
		includeHandler.dxcUtils = dxcUtils.Get();

		Microsoft::WRL::ComPtr<IDxcResult> dxcResult;
		const HRESULT compilerCallHResult = dxcCompiler->Compile(&dxcMainSourceBuffer,
			dxcArgsList.getData(), dxcArgsList.getSize(), &includeHandler, IID_PPV_ARGS(&dxcResult));

		if (FAILED(compilerCallHResult) || !dxcResult)
		{
			ShaderCompilationResult::ComposerSource resultComposerSrc = {};
			resultComposerSrc.status = ShaderCompilationStatus::CompilerCallFailed;
			return ShaderCompilationResult::Compose(resultComposerSrc);
		}

		XAssert(includeHandler.refCount == 0);
		
		dxcResult->GetStatus(&preprocessingStatusHResult);
		dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&dxcPreprocessingErrorsBlob), nullptr);
		dxcResult->GetOutput(DXC_OUT_HLSL, IID_PPV_ARGS(&dxcPreprocessedSourceBlob), nullptr);
	}

	if (FAILED(preprocessingStatusHResult))
	{
		ShaderCompilationResult::ComposerSource resultComposerSrc = {};
		resultComposerSrc.status = ShaderCompilationStatus::PreprocessingError;
		resultComposerSrc.preprocessingOuputStr = StringViewASCII(
			dxcPreprocessingErrorsBlob->GetStringPointer(), dxcPreprocessingErrorsBlob->GetStringLength());
		return ShaderCompilationResult::Compose(resultComposerSrc);
	}


	// Patch source ////////////////////////////////////////////////////////////////////////////////
	DynamicStringASCII patchedSource;
	{
		StringViewASCII preprocessedSource(
			dxcPreprocessedSourceBlob->GetStringPointer(), dxcPreprocessedSourceBlob->GetStringLength());

		HLSLPatcher::Error hlslPatcherError = {};
		if (!HLSLPatcher::Patch(preprocessedSource, pipelineLayout, patchedSource, hlslPatcherError))
		{
			XTODO(__FUNCTION__": HLSL patching: cursor location is invalid due to preprocessing. Parse #line directives");

			InplaceStringASCIIx2048 xePreprocessorOutput;
			FmtPrintStr(xePreprocessorOutput, mainSourceFilename, ":",
				hlslPatcherError.location.lineNumber, ":", hlslPatcherError.location.columnNumber,
				": XE HLSL patcher: error: ", hlslPatcherError.message);

			ShaderCompilationResult::ComposerSource resultComposerSrc = {};
			resultComposerSrc.status = ShaderCompilationStatus::PreprocessingError;
			resultComposerSrc.preprocessingOuputStr = xePreprocessorOutput;
			return ShaderCompilationResult::Compose(resultComposerSrc);
		}
	}


	// Actually compile shader via DXC /////////////////////////////////////////////////////////////
	HRESULT compilerCallHResult = E_FAIL;
	HRESULT compilationStatusHResult = E_FAIL;
	Microsoft::WRL::ComPtr<IDxcBlobUtf8> dxcCompilationErrorsBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> dxcBytecodeBlob;
	{
		// Build argument list
		InplaceArrayList<LPCWSTR, 64> dxcArgsList;
		{
			dxcArgsList.pushBack(mainSourceFilenameW);

			LPCWSTR dxcProfile = nullptr;
			switch (args.shaderType)
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

		DxcBuffer dxcSourceBuffer = {};
		dxcSourceBuffer.Ptr = patchedSource.getData();
		dxcSourceBuffer.Size = patchedSource.getLength();
		dxcSourceBuffer.Encoding = CP_UTF8;

		Microsoft::WRL::ComPtr<IDxcResult> dxcResult;
		compilerCallHResult = dxcCompiler->Compile(&dxcSourceBuffer,
			dxcArgsList.getData(), dxcArgsList.getSize(), nullptr, IID_PPV_ARGS(&dxcResult));

		if (dxcResult)
		{
			dxcResult->GetStatus(&compilationStatusHResult);
			dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&dxcCompilationErrorsBlob), nullptr);
			dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxcBytecodeBlob), nullptr);
		}
	}


	// Compose shader compilation result ///////////////////////////////////////////////////////////
	ShaderCompilationResult::ComposerSource resultComposerSrc = {};

	if (FAILED(compilerCallHResult))
		resultComposerSrc.status = ShaderCompilationStatus::CompilerCallFailed;
	else if (FAILED(compilationStatusHResult))
		resultComposerSrc.status = ShaderCompilationStatus::CompilationError;
	else
		resultComposerSrc.status = ShaderCompilationStatus::Success;

	resultComposerSrc.preprocessingOuputStr = StringViewASCII(
		dxcPreprocessingErrorsBlob->GetStringPointer(), dxcPreprocessingErrorsBlob->GetStringLength());
	resultComposerSrc.compilationOutputStr = StringViewASCII(
		dxcCompilationErrorsBlob->GetStringPointer(), dxcCompilationErrorsBlob->GetStringLength());

	BlobRef preprocessedSourceBlob = nullptr;
	if (!patchedSource.isEmpty())
	{
		preprocessedSourceBlob = Blob::Create(patchedSource.getLength());
		memoryCopy((void*)preprocessedSourceBlob->getData(), patchedSource.getData(), patchedSource.getLength());

		resultComposerSrc.preprocessedSourceBlob = preprocessedSourceBlob.get();
	}

	BlobRef bytecodeBlob = nullptr;
	if (dxcBytecodeBlob && dxcBytecodeBlob->GetBufferSize() > 0)
	{
		const uint32 bytecodeSize = XCheckedCastU32(dxcBytecodeBlob->GetBufferSize());

		// Compose bytecode blob
		BlobFormat::ShaderBlobInfo blobInfo = {};
		blobInfo.pipelineLayoutSourceHash = pipelineLayout.getSourceHash();
		blobInfo.bytecodeSize = bytecodeSize;
		blobInfo.shaderType = args.shaderType;

		BlobFormat::ShaderBlobWriter blobWriter;
		blobWriter.setup(blobInfo);

		bytecodeBlob = Blob::Create(blobWriter.getBlobSize());
		blobWriter.setBlobMemory((void*)bytecodeBlob->getData(), bytecodeBlob->getSize());
		blobWriter.writeBytecode(dxcBytecodeBlob->GetBufferPointer(), bytecodeSize);
		blobWriter.finalize();

		resultComposerSrc.bytecodeBlob = bytecodeBlob.get();
	}

	return ShaderCompilationResult::Compose(resultComposerSrc);
}
