#include <XLib.String.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.XStringHash.h>

#include "XEngine.Render.Shaders.Builder.PipelineLayoutList.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder_;

bool PipelineLayout::compile()
{
	return HAL::ShaderCompiler::Host::CompilePipelineLayout(HAL::ShaderCompiler::Platform::D3D12,
		bindPoints, bindPointCount, compiledPipelineLayout);
}

PipelineLayoutCreationResult PipelineLayoutList::create(XLib::StringViewASCII name,
	const HAL::ShaderCompiler::PipelineBindPointDesc* bindPoints, uint8 bindPointCount)
{
	if (bindPointCount > HAL::MaxPipelineBindPointCount)
		return PipelineLayoutCreationResult { PipelineLayoutCreationStatus::Failure_TooManyBindPoints };

	const uint64 nameXSH = XStringHash::Compute(name);
	if (entrySearchTree.find(nameXSH))
	{
		// Duplicate name found or hash collision. These cases should be handled separately.
		return PipelineLayoutCreationResult { PipelineLayoutCreationStatus::Failure_EntryNameDuplication };
	}

	// Calculate memory block size.
	uint32 memoryBlockSizeAccum = sizeof(PipelineLayout);
	memoryBlockSizeAccum = alignUp<uint32>(memoryBlockSizeAccum, 16);

	const uint32 bindPointsMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(HAL::ShaderCompiler::PipelineBindPointDesc) * bindPointCount;

	const uint32 nameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += uint32(name.getLength()) + 1;

	uint32 bindPointNamesMemOffset[HAL::MaxPipelineBindPointCount] = {};
	for (uint8 i = 0; i < bindPointCount; i++)
	{
		bindPointNamesMemOffset[i] = memoryBlockSizeAccum;
		memoryBlockSizeAccum += uint32(bindPoints[i].name.getLength()) + 1;
	}

	// Allocate and populate memory block.
	const uint32 memoryBlockSize = memoryBlockSizeAccum;
	byte* pipelineLayoutMemory = (byte*)SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(pipelineLayoutMemory, 0, memoryBlockSize);

	char* nameMemory = (char*)(pipelineLayoutMemory + nameMemOffset);
	memoryCopy(nameMemory, name.getData(), name.getLength());
	nameMemory[name.getLength()] = '\0';

	HAL::ShaderCompiler::PipelineBindPointDesc* bindPointsMemory =
		(HAL::ShaderCompiler::PipelineBindPointDesc*)(pipelineLayoutMemory + bindPointsMemOffset);
	for (uint8 i = 0; i < bindPointCount; i++)
	{
		const HAL::ShaderCompiler::PipelineBindPointDesc& srcBindPoint = bindPoints[i];
		HAL::ShaderCompiler::PipelineBindPointDesc& dstBindPoint = bindPointsMemory[i];
		const StringViewASCII& srcBindPointName = bindPoints[i].name;

		char* bindPointNameMemory = (char*)(pipelineLayoutMemory + bindPointNamesMemOffset[i]);
		memoryCopy(bindPointNameMemory, srcBindPointName.getData(), srcBindPointName.getLength());
		bindPointNameMemory[srcBindPointName.getLength()] = '\0';

		dstBindPoint = srcBindPoint;
		dstBindPoint.name = StringView(bindPointNameMemory, srcBindPointName.getLength());
	}

	PipelineLayout& pipelineLayout = *(PipelineLayout*)pipelineLayoutMemory;
	construct(pipelineLayout);
	pipelineLayout.name = StringView(nameMemory, name.getLength());
	pipelineLayout.nameXSH = nameXSH;
	pipelineLayout.bindPoints = bindPointsMemory;
	pipelineLayout.bindPointCount = bindPointCount;

	entrySearchTree.insert(pipelineLayout);
	entryCount++;

	return PipelineLayoutCreationResult { PipelineLayoutCreationStatus::Success, &pipelineLayout };
}

PipelineLayout* PipelineLayoutList::find(XLib::StringViewASCII name) const
{
	return entrySearchTree.find(XStringHash::Compute(name));
}
