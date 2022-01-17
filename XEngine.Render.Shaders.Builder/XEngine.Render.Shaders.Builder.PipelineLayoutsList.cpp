#include <XLib.CRC.h>
#include <XLib.String.h>

#include "XEngine.Render.Shaders.Builder.PipelineLayoutsList.h"

using namespace XLib;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::Shaders::Builder_;

bool PipelineLayout::compile()
{
	ShaderCompiler::PipelineBindPointDesc compilerBindPointDescs[MaxPipelineBindPointCount] = {};
	XAssert(bindPointCount <= countof(compilerBindPointDescs));
	XAssert(bindPointsOffsetInParentStorage + bindPointCount <= parentList.bindPointsStorageList.getSize());

	for (uint8 i = 0; i < bindPointCount; i++)
	{
		const BindPointDesc& src = parentList.bindPointsStorageList[bindPointsOffsetInParentStorage + i];
		ShaderCompiler::PipelineBindPointDesc& dst = compilerBindPointDescs[i];
		dst.name = src.name;
		dst.type = src.type;
		dst.shaderVisibility = src.shaderVisibility;
		dst.constantsSize32bitValues = src.constantsSize32bitValues;
	}

	return ShaderCompiler::Host::CompilePipelineLayout(ShaderCompiler::Platform::D3D12,
		compilerBindPointDescs, bindPointCount, compiledPipelineLayout);
}

PipelineLayout* PipelineLayoutsList::createEntry(const char* name, const BindPointDesc* bindPoints, uint8 bindPointCount)
{
	XAssert(bindPointCount <= MaxPipelineBindPointCount); // TODO: Make this an error.

	const uintptr nameLength = GetCStrLength(name);
	const uint64 nameCRC = CRC64::Compute(name, nameLength);

	if (entriesSearchTree.find(nameCRC))
		return nullptr; // Duplicate name found or CRC collision (can be handled separately).

	const uint32 bindPointsOffset = bindPointsStorageList.getSize();
	for (uint8 i = 0; i < bindPointCount; i++)
		bindPointsStorageList.pushBack(bindPoints[i]);

	PipelineLayout& pipelineLayout = entriesStorageList.emplaceBack(*this);
	pipelineLayout.name = name;
	pipelineLayout.nameCRC = nameCRC;
	pipelineLayout.bindPointsOffsetInParentStorage = bindPointsOffset;
	pipelineLayout.bindPointCount = bindPointCount;

	entriesSearchTree.insert(pipelineLayout);

	return &pipelineLayout;
}
