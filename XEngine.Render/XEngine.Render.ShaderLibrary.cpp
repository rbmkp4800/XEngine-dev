#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Render.Shaders.PackFormat.h>

#include "XEngine.Render.h"

#include "XEngine.Render.ShaderLibrary.h"

using namespace XLib;
using namespace XEngine::Render;

// TODO: Implement proper search for `getPipelineLayout` and `getPipeline`.

struct ShaderLibrary::PipelineLayout
{
	uint64 nameXSH;
	HAL::PipelineLayoutHandle halPipelineLayout;
};

struct ShaderLibrary::Pipeline
{
	uint64 nameXSH;
	HAL::PipelineHandle halPipeline;
};

ShaderLibrary::~ShaderLibrary()
{
	if (pipelineLayoutTable)
	{
		XEAssert(pipelineTable);

		// TODO: Do this properly.
		SystemHeapAllocator::Release(pipelineLayoutTable);

		pipelineLayoutTable = nullptr;
		pipelineTable = nullptr;
		pipelineLayoutCount = 0;
		pipelineCount = 0;
	}
}

void ShaderLibrary::load(const char* packPath)
{
	// TODO: Refactor this method.

	HAL::Device& halDevice = Host::GetDevice();

	using namespace Shaders::PackFormat;

	File packFile;
	packFile.open(packPath, FileAccessMode::Read, FileOpenMode::OpenExisting);
	XEMasterAssert(packFile.isInitialized());

	const uint64 packFileSize = packFile.getSize();
	XEMasterAssert(packFileSize > sizeof(PackHeader));

	PackHeader packHeader = {};
	packFile.read(packHeader);
	XEMasterAssert(packHeader.signature == PackSignature);
	XEMasterAssert(packHeader.version == PackCurrentVersion);
	XEMasterAssert(packHeader.pipelineLayoutCount > 0);
	XEMasterAssert(packHeader.pipelineCount > 0);
	XEMasterAssert(packHeader.bytecodeObjectCount > 0);

	const uint32 objectsBaseOffsetCheck = sizeof(PackHeader) +
		sizeof(PipelineLayoutRecord) * packHeader.pipelineLayoutCount +
		sizeof(PipelineRecord) * packHeader.pipelineCount +
		sizeof(ObjectRecord) * packHeader.bytecodeObjectCount;
	XEMasterAssert(packHeader.objectsBaseOffset == objectsBaseOffsetCheck);
	XEMasterAssert(packHeader.objectsBaseOffset < packFileSize);

	pipelineLayoutCount = packHeader.pipelineLayoutCount;
	pipelineCount = packHeader.pipelineCount;

	// Allocate tables.
	{
		// TODO: Handle this memory in proper way.
		const uintptr tablesMemorySize = sizeof(PipelineLayout) * pipelineLayoutCount + sizeof(Pipeline) * pipelineCount;
		void* tablesMemory = SystemHeapAllocator::Allocate(tablesMemorySize);
		memorySet(tablesMemory, 0, tablesMemorySize);

		pipelineLayoutTable = (PipelineLayout*)tablesMemory;
		pipelineTable = (Pipeline*)(pipelineLayoutTable + pipelineLayoutCount);
		void* tablesMemoryEnd = pipelineTable + pipelineCount;

		XEAssert(uintptr(tablesMemory) + tablesMemorySize == uintptr(tablesMemoryEnd));
	}

	byte* packData = (byte*)SystemHeapAllocator::Allocate(packFileSize);

	memoryCopy(packData, &packHeader, sizeof(PackHeader)); // Pack header is already read.
	packFile.read(packData + sizeof(PackHeader), packFileSize - sizeof(PackHeader));

	const PipelineLayoutRecord* pipelineLayoutRecords = (PipelineLayoutRecord*)(packData + sizeof(PackHeader));
	const PipelineRecord* pipelineRecords = (PipelineRecord*)(pipelineLayoutRecords + packHeader.pipelineLayoutCount);
	const ObjectRecord* bytecodeObjectRecords = (ObjectRecord*)(pipelineRecords + packHeader.pipelineCount);
	const byte* objectsDataBegin = (byte*)(bytecodeObjectRecords + packHeader.bytecodeObjectCount);
	XEAssert(uintptr(objectsDataBegin) == uintptr(packData) + objectsBaseOffsetCheck);

	byte* packDataEnd = packData + packFileSize;
	XEAssert(objectsDataBegin < packDataEnd);
	const uint64 objectsDataSize = packDataEnd - objectsDataBegin;

	uint64 prevNameXSH = 0;

	for (uint16 pipelineLayoutIndex = 0; pipelineLayoutIndex < packHeader.pipelineLayoutCount; pipelineLayoutIndex++)
	{
		const PipelineLayoutRecord& pipelineLayoutRecord = pipelineLayoutRecords[pipelineLayoutIndex];
		PipelineLayout& pipelineLayout = pipelineLayoutTable[pipelineLayoutIndex];

		HAL::ObjectDataView pipelineLayoutObject = {};
		pipelineLayoutObject.data = objectsDataBegin + pipelineLayoutRecord.object.offset;
		pipelineLayoutObject.size = pipelineLayoutRecord.object.size;

		pipelineLayout.halPipelineLayout = halDevice.createPipelineLayout(pipelineLayoutObject);
		pipelineLayout.nameXSH = pipelineLayoutRecord.nameXSH;

		XEMasterAssert(prevNameXSH < pipelineLayout.nameXSH); // Check order.
		prevNameXSH = pipelineLayout.nameXSH;
	}

	prevNameXSH = 0;

	for (uint16 pipelineIndex = 0; pipelineIndex < packHeader.pipelineCount; pipelineIndex++)
	{
		const PipelineRecord& pipelineRecord = pipelineRecords[pipelineIndex];
		Pipeline& pipeline = pipelineTable[pipelineIndex];

		XEMasterAssert(pipelineRecord.pipelineLayoutIndex < pipelineLayoutCount);
		const PipelineLayout& pipelineLayout = pipelineLayoutTable[pipelineRecord.pipelineLayoutIndex];

		if (pipelineRecord.isGraphics)
		{
			XEMasterAssert(
				pipelineRecord.graphicsBaseObject.offset + pipelineRecord.graphicsBaseObject.size <= objectsDataSize);

			HAL::ObjectDataView graphicsBaseObject = {};
			graphicsBaseObject.data = objectsDataBegin + pipelineRecord.graphicsBaseObject.offset;
			graphicsBaseObject.size = pipelineRecord.graphicsBaseObject.size;

			HAL::ObjectDataView bytecodeObjects[HAL::MaxGraphicsPipelineBytecodeObjectCount] = {};
			uint8 bytecodeObjectCount = 0;

			// Fill `bytecodeObjects`.
			bool invalidBytecodeObjectIndexReached = false;
			for (uint8 i = 0; i < countof(pipelineRecord.bytecodeObjectsIndices); i++)
			{
				const uint16 bytecodeObjectIndex = pipelineRecord.bytecodeObjectsIndices[i];
				if (bytecodeObjectIndex == uint16(-1))
				{
					invalidBytecodeObjectIndexReached = true;
					continue;
				}

				XEMasterAssert(!invalidBytecodeObjectIndexReached);
				XEMasterAssert(i < HAL::MaxGraphicsPipelineBytecodeObjectCount);
				XEMasterAssert(bytecodeObjectIndex < packHeader.bytecodeObjectCount);
				const ObjectRecord& bytecodeObjectRecord = bytecodeObjectRecords[bytecodeObjectIndex];

				XEMasterAssert(bytecodeObjectRecord.offset + bytecodeObjectRecord.size <= objectsDataSize);

				bytecodeObjects[i].data = objectsDataBegin + bytecodeObjectRecord.offset;
				bytecodeObjects[i].size = bytecodeObjectRecord.size;
				bytecodeObjectCount++;
			}

			XEMasterAssert(bytecodeObjectCount > 0);

			pipeline.halPipeline = halDevice.createGraphicsPipeline(
				pipelineLayout.halPipelineLayout,
				graphicsBaseObject, bytecodeObjects, bytecodeObjectCount,
				HAL::RasterizerDesc{}, HAL::DepthStencilDesc{}, HAL::BlendDesc{});
		}
		else
		{
			// TODO: Asserts everywhere.
			// TODO: Check that all bytecode object indicies after first are invalid.
			const uint16 computeShaderObjectIndex = pipelineRecord.bytecodeObjectsIndices[0];
			XEMasterAssert(computeShaderObjectIndex < packHeader.bytecodeObjectCount);
			const ObjectRecord& computeShaderObjectRecord = bytecodeObjectRecords[computeShaderObjectIndex];

			XEMasterAssert(computeShaderObjectRecord.offset + computeShaderObjectRecord.size <= objectsDataSize);

			HAL::ObjectDataView computeShaderObject = {};
			computeShaderObject.data = objectsDataBegin + computeShaderObjectRecord.offset;
			computeShaderObject.size = computeShaderObjectRecord.size;

			pipeline.halPipeline = halDevice.createComputePipeline(
				pipelineLayout.halPipelineLayout, computeShaderObject);
		}

		pipeline.nameXSH = pipelineRecord.nameXSH;

		// Check ordering of pipelines.
		XEMasterAssert(prevNameXSH < pipeline.nameXSH);
		prevNameXSH = pipeline.nameXSH;
	}

	SystemHeapAllocator::Release(packData);
}

HAL::PipelineLayoutHandle ShaderLibrary::getPipelineLayout(uint64 nameXSH) const
{
	for (uint32 i = 0; i < pipelineLayoutCount; i++)
	{
		if (pipelineLayoutTable[i].nameXSH == nameXSH)
			return pipelineLayoutTable[i].halPipelineLayout;
	}
	XEMasterAssertUnreachableCode();
}

HAL::PipelineHandle ShaderLibrary::getPipeline(uint64 nameXSH) const
{
	for (uint32 i = 0; i < pipelineCount; i++)
	{
		if (pipelineTable[i].nameXSH == nameXSH)
			return pipelineTable[i].halPipeline;
	}
	XEMasterAssertUnreachableCode();
}
