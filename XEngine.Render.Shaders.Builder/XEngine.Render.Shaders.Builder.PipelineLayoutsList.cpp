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
		const PipelineLayoutsList::InplaceBindPointDesc& src =
			parentList.bindPointsStorageList[bindPointsOffsetInParentStorage + i];
		ShaderCompiler::PipelineBindPointDesc& dst = compilerBindPointDescs[i];
		dst.name = src.name;
		dst.type = src.type;
		dst.shaderVisibility = src.shaderVisibility;
		dst.constantsSize32bitValues = src.constantsSize32bitValues;
	}

	return ShaderCompiler::Host::CompilePipelineLayout(ShaderCompiler::Platform::D3D12,
		compilerBindPointDescs, bindPointCount, compiledPipelineLayout);
}

PipelineLayoutsList::EntryCreationResult PipelineLayoutsList::createEntry(
	XLib::StringView name, const BindPointDesc* bindPoints, uint8 bindPointCount)
{
	XAssert(bindPointCount <= MaxPipelineBindPointCount);

	const uint64 nameCRC = CRC64::Compute(name.getData(), name.getLength());
	if (entriesSearchTree.find(nameCRC))
	{
		// Duplicate name found or CRC collision. These cases should be handled separately.
		return EntryCreationResult { nullptr, EntryCreationStatus::Failure_EntryWithSuchNameAlreadyExists };
	}

	const uint32 bindPointsOffset = bindPointsStorageList.getSize();
	for (uint8 i = 0; i < bindPointCount; i++)
	{
		const BindPointDesc& src = bindPoints[i];
		InplaceBindPointDesc& dst = bindPointsStorageList.emplaceBack();

		XAssert(src.name.getLength() <= dst.name.GetMaxLength());
		dst.name.copyFrom(src.name);
		dst.type = src.type;
		dst.shaderVisibility = src.shaderVisibility;
		dst.constantsSize32bitValues = src.constantsSize32bitValues;
	}

	PipelineLayout& pipelineLayout = entriesStorageList.emplaceBack(*this);
	pipelineLayout.name.copyFrom(name);
	pipelineLayout.nameCRC = nameCRC;
	pipelineLayout.bindPointsOffsetInParentStorage = bindPointsOffset;
	pipelineLayout.bindPointCount = bindPointCount;

	entriesSearchTree.insert(pipelineLayout);

	return EntryCreationResult { &pipelineLayout, EntryCreationStatus::Success };
}

PipelineLayout* PipelineLayoutsList::findEntry(XLib::StringView name) const
{
	const uint64 nameCRC = CRC64::Compute(name.getData(), name.getLength());
	return entriesSearchTree.find(nameCRC);
}
