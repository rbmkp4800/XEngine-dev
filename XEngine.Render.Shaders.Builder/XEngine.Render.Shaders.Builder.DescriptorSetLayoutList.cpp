#include <XLib.SystemHeapAllocator.h>

#include <XEngine.XStringHash.h>

#include "XEngine.Render.Shaders.Builder.DescriptorSetLayoutList.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder_;

bool DescriptorSetLayout::compile()
{
	return HAL::ShaderCompiler::Host::CompileDescriptorSetLayout(HAL::ShaderCompiler::Platform::D3D12,
		bindings, bindingCount, compiledDescriptorSetLayout);
}

DescriptorSetLayoutCreationResult DescriptorSetLayoutList::create(XLib::StringViewASCII name,
	const HAL::ShaderCompiler::DescriptorSetNestedBindingDesc* bindings, uint8 bindingCount)
{
	if (bindingCount > HAL::MaxDescriptorSetNestedBindingCount)
		return DescriptorSetLayoutCreationResult { DescriptorSetLayoutCreationStatus::Failure_TooManyBindings };

	const uint64 nameXSH = XStringHash::Compute(name);
	if (entrySearchTree.find(nameXSH))
	{
		// Duplicate name found or hash collision. These cases should be handled separately.
		return DescriptorSetLayoutCreationResult { DescriptorSetLayoutCreationStatus::Failure_EntryNameDuplication };
	}

	// Calculate memory block size.
	uint32 memoryBlockSizeAccum = sizeof(DescriptorSetLayout);
	memoryBlockSizeAccum = alignUp<uint32>(memoryBlockSizeAccum, 16);

	const uint32 bindingsMemoryOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(HAL::ShaderCompiler::DescriptorSetNestedBindingDesc) * bindingCount;

	const uint32 nameMemoryOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += uint32(name.getLength()) + 1;

	uint32 bindingNamesMemoryOffsets[HAL::MaxDescriptorSetNestedBindingCount] = {};
	for (uint8 i = 0; i < bindingCount; i++)
	{
		bindingNamesMemoryOffsets[i] = memoryBlockSizeAccum;
		memoryBlockSizeAccum += uint32(bindings[i].name.getLength()) + 1;
	}

	// Allocate and populate memory block.
	const uint32 memoryBlockSize = memoryBlockSizeAccum;
	byte* descriptorSetLayoutMemory = (byte*)SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(descriptorSetLayoutMemory, 0, memoryBlockSize);

	char* nameMemory = (char*)(descriptorSetLayoutMemory + nameMemoryOffset);
	memoryCopy(nameMemory, name.getData(), name.getLength());
	nameMemory[name.getLength()] = '\0';

	HAL::ShaderCompiler::DescriptorSetNestedBindingDesc* bindingsMemory =
		(HAL::ShaderCompiler::DescriptorSetNestedBindingDesc*)(descriptorSetLayoutMemory + bindingsMemoryOffset);
	for (uint8 i = 0; i < bindingCount; i++)
	{
		const HAL::ShaderCompiler::DescriptorSetNestedBindingDesc& srcBinding = bindings[i];
		HAL::ShaderCompiler::DescriptorSetNestedBindingDesc& dstBinding = bindingsMemory[i];

		char* bindingNameMemory = (char*)(descriptorSetLayoutMemory + bindingNamesMemoryOffsets[i]);
		memoryCopy(bindingNameMemory, srcBinding.name.getData(), srcBinding.name.getLength());
		bindingNameMemory[srcBinding.name.getLength()] = '\0';

		dstBinding = srcBinding;
		dstBinding.name = StringView(bindingNameMemory, srcBinding.name.getLength());
	}

	DescriptorSetLayout& descriptorSetLayout = *(DescriptorSetLayout*)descriptorSetLayoutMemory;
	construct(descriptorSetLayout);
	descriptorSetLayout.name = StringView(nameMemory, name.getLength());
	descriptorSetLayout.nameXSH = nameXSH;
	descriptorSetLayout.bindings = bindingsMemory;
	descriptorSetLayout.bindingCount = bindingCount;

	entrySearchTree.insert(descriptorSetLayout);
	entryCount++;

	return DescriptorSetLayoutCreationResult { DescriptorSetLayoutCreationStatus::Success, &descriptorSetLayout };
}

DescriptorSetLayout* DescriptorSetLayoutList::find(XLib::StringViewASCII name) const
{
	return entrySearchTree.find(XStringHash::Compute(name));
}
