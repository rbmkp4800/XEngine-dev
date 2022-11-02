#include <XLib.SystemHeapAllocator.h>

#include <XEngine.XStringHash.h>

#include "XEngine.Render.Shaders.Builder.PipelineLayoutList.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder_;

bool PipelineLayout::compile()
{
	HAL::ShaderCompiler::PipelineBindingDesc halBindings[HAL::MaxPipelineBindingCount] = {};

	for (uint8 i = 0; i < bindingCount; i++)
	{
		const PipelineBindingDesc& srcBinding = bindings[i];
		HAL::ShaderCompiler::PipelineBindingDesc& dstBinding = halBindings[i];

		dstBinding.name = srcBinding.name;
		dstBinding.type = srcBinding.type;
		if (srcBinding.type == HAL::PipelineBindingType::Constants)
			dstBinding.constantsSize = srcBinding.constantsSize;
		else if (srcBinding.type == HAL::PipelineBindingType::DescriptorSet)
		{
			dstBinding.compiledDescriptorSetLayout = &srcBinding.descriptorSetLayout->getCompiledBlob();
			XAssert(dstBinding.compiledDescriptorSetLayout->isInitialized());
		}
	}

	return HAL::ShaderCompiler::Host::CompilePipelineLayout(HAL::ShaderCompiler::Platform::D3D12,
		halBindings, bindingCount, compiledPipelineLayoutBlob, pipelineLayoutMetadataBlob);
}

PipelineLayoutCreationResult PipelineLayoutList::create(XLib::StringViewASCII name,
	const PipelineBindingDesc* bindings, uint8 bindingCount)
{
	if (bindingCount > HAL::MaxPipelineBindingCount)
		return PipelineLayoutCreationResult { PipelineLayoutCreationStatus::Failure_TooManyBindings };

	const uint64 nameXSH = XStringHash::Compute(name);
	if (entrySearchTree.find(nameXSH))
	{
		// Duplicate name found or hash collision. These cases should be handled separately.
		return PipelineLayoutCreationResult { PipelineLayoutCreationStatus::Failure_EntryNameDuplication };
	}

	// Calculate memory block size.
	uint32 memoryBlockSizeAccum = sizeof(PipelineLayout);
	memoryBlockSizeAccum = alignUp<uint32>(memoryBlockSizeAccum, 16);

	const uint32 bindingsMemoryOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(PipelineBindingDesc) * bindingCount;

	const uint32 nameMemoryOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += uint32(name.getLength()) + 1;

	uint32 bindingNamesMemoryOffsets[HAL::MaxPipelineBindingCount] = {};
	for (uint8 i = 0; i < bindingCount; i++)
	{
		bindingNamesMemoryOffsets[i] = memoryBlockSizeAccum;
		memoryBlockSizeAccum += uint32(bindings[i].name.getLength()) + 1;
	}

	// Allocate and populate memory block.
	const uint32 memoryBlockSize = memoryBlockSizeAccum;
	byte* pipelineLayoutMemory = (byte*)SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(pipelineLayoutMemory, 0, memoryBlockSize);

	char* nameMemory = (char*)(pipelineLayoutMemory + nameMemoryOffset);
	memoryCopy(nameMemory, name.getData(), name.getLength());
	nameMemory[name.getLength()] = '\0';

	PipelineBindingDesc* bindingsMemory =
		(PipelineBindingDesc*)(pipelineLayoutMemory + bindingsMemoryOffset);
	for (uint8 i = 0; i < bindingCount; i++)
	{
		const PipelineBindingDesc& srcBinding = bindings[i];
		PipelineBindingDesc& dstBinding = bindingsMemory[i];

		char* bindingNameMemory = (char*)(pipelineLayoutMemory + bindingNamesMemoryOffsets[i]);
		memoryCopy(bindingNameMemory, srcBinding.name.getData(), srcBinding.name.getLength());
		bindingNameMemory[srcBinding.name.getLength()] = '\0';

		dstBinding = srcBinding;
		dstBinding.name = StringView(bindingNameMemory, srcBinding.name.getLength());
	}

	PipelineLayout& pipelineLayout = *(PipelineLayout*)pipelineLayoutMemory;
	construct(pipelineLayout);
	pipelineLayout.name = StringView(nameMemory, name.getLength());
	pipelineLayout.nameXSH = nameXSH;
	pipelineLayout.bindings = bindingsMemory;
	pipelineLayout.bindingCount = bindingCount;

	entrySearchTree.insert(pipelineLayout);
	entryCount++;

	return PipelineLayoutCreationResult { PipelineLayoutCreationStatus::Success, &pipelineLayout };
}

PipelineLayout* PipelineLayoutList::find(XLib::StringViewASCII name) const
{
	return entrySearchTree.find(XStringHash::Compute(name));
}
