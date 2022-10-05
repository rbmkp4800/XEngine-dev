#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Render.Shaders.PackFormat.h>

#include "XEngine.Render.h"

#include "XEngine.Render.ShaderLibrary.h"

using namespace XLib;
using namespace XEngine::Render;

struct ShaderLibrary::PipelineLayout
{
	uint64 nameCRC;
	HAL::PipelineLayoutHandle halPipelineLayout;
};

struct ShaderLibrary::Pipeline
{
	uint64 nameCRC;
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
	XEMasterAssert(packFileSize > packHeader.objectsBaseOffset);

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

	const uint32 packSizeWithoutHeader = uint32(packFileSize) - sizeof(PackHeader);
	byte* packData = (byte*)SystemHeapAllocator::Allocate(packSizeWithoutHeader);

	packFile.read(packData, packSizeWithoutHeader);
	const PipelineLayoutRecord* pipelineLayoutRecords = (PipelineLayoutRecord*)packData;
	const PipelineRecord* pipelineRecords = (PipelineRecord*)(pipelineLayoutRecords + packHeader.pipelineLayoutCount);
	const ObjectRecord* bytecodeObjectRecords = (ObjectRecord*)(pipelineRecords + packHeader.pipelineCount);
	const byte* objectsDataBegin = (byte*)(bytecodeObjectRecords + packHeader.bytecodeObjectCount);

	uint64 prevNameCRC = 0;

	for (uint16 pipelineLayoutIndex = 0; pipelineLayoutIndex < packHeader.pipelineLayoutCount; pipelineLayoutIndex++)
	{
		const PipelineLayoutRecord& pipelineLayoutRecord = pipelineLayoutRecords[pipelineLayoutIndex];
		PipelineLayout& pipelineLayout = pipelineLayoutTable[pipelineLayoutIndex];

		HAL::ObjectDataView pipelineLayoutObject = {};
		pipelineLayoutObject.data = objectsDataBegin + pipelineLayoutRecord.object.offset;
		pipelineLayoutObject.size = pipelineLayoutRecord.object.size;

		pipelineLayout.halPipelineLayout = halDevice.createPipelineLayout(pipelineLayoutObject);
		pipelineLayout.nameCRC = pipelineLayoutRecord.nameCRC;

		XEMasterAssert(prevNameCRC < pipelineLayout.nameCRC); // Check order.
		prevNameCRC = pipelineLayout.nameCRC;
	}

	prevNameCRC = 0;

	for (uint16 pipelineIndex = 0; pipelineIndex < packHeader.pipelineCount; pipelineIndex++)
	{
		const PipelineRecord& pipelineRecord = pipelineRecords[pipelineIndex];
		Pipeline& pipeline = pipelineTable[pipelineIndex];

		XEMasterAssert(pipelineRecord.pipelineLayoutIndex < pipelineLayoutCount);
		const PipelineLayout& pipelineLayout = pipelineLayoutTable[pipelineRecord.pipelineLayoutIndex];

		if (pipelineRecord.isGraphics)
		{
			// TODO: Check `pipelineRecord.graphicsBaseObject`.

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
			const uint16 computeShaderBytecodeObjectIndex = pipelineRecord.bytecodeObjectsIndices[0];
			XEMasterAssert(computeShaderBytecodeObjectIndex < packHeader.bytecodeObjectCount);
			const ObjectRecord& computeShaderBytecodeObjectRecord = bytecodeObjectRecords[computeShaderBytecodeObjectIndex];

			HAL::ObjectDataView computeShaderBytecodeObject = {};
			computeShaderBytecodeObject.data = objectsDataBegin + computeShaderBytecodeObjectRecord.size;
			computeShaderBytecodeObject.size = computeShaderBytecodeObjectRecord.size;

			pipeline.halPipeline = halDevice.createComputePipeline(
				pipelineLayout.halPipelineLayout, computeShaderBytecodeObject);
		}

		pipeline.nameCRC = pipelineRecord.nameCRC;

		// Check ordering of pipelines.
		XEMasterAssert(prevNameCRC < pipeline.nameCRC);
		prevNameCRC = pipeline.nameCRC;
	}

	SystemHeapAllocator::Release(packData);
}
