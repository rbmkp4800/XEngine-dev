#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Render.HAL.Common.h>
#include <XEngine.Render.Shaders.PackFormat.h>

#include "XEngine.Render.Device.TextureHeap.h"

#include "XEngine.Render.Device.h"

using namespace XLib;
using namespace XEngine::Render;
using namespace XEngine::Render::Device_;

void ShadersLoader::load(const char* packPath)
{
	// TODO: Replace all `XAssert`s with `XEMasterAssert`

	using namespace Shaders::PackFormat;

	File packFile;
	packFile.open(packPath, FileAccessMode::Read, FileOpenMode::OpenExisting);
	XAssert(packFile.isInitialized());

	const uint64 packFileSize = packFile.getSize();
	XAssert(packFileSize > sizeof(PackHeader));
	
	PackHeader packHeader = {};
	packFile.read(packHeader);
	XAssert(packHeader.signature == PackSignature);
	XAssert(packHeader.version == PackCurrentVersion);
	XAssert(packHeader.pipelineLayoutCount > 0);
	XAssert(packHeader.pipelineCount > 0);
	XAssert(packHeader.bytecodeObjectCount > 0);

	const uint32 objectsBaseOffsetCheck = sizeof(PackHeader) +
		sizeof(PipelineLayoutRecord) * packHeader.pipelineLayoutCount +
		sizeof(PipelineRecord) * packHeader.pipelineCount +
		sizeof(ObjectRecord) * packHeader.bytecodeObjectCount;
	XAssert(packHeader.objectsBaseOffset == objectsBaseOffsetCheck);
	XAssert(packFileSize > packHeader.objectsBaseOffset);

	pipelineLayoutsList.resize(packHeader.pipelineLayoutCount);
	pipelinesList.resize(packHeader.pipelineCount);

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
		PipelineLayout& pipelineLayout = pipelineLayoutsList[pipelineLayoutIndex];

		HAL::ObjectDataView pipelineLayoutObject = {};
		pipelineLayoutObject.data = objectsDataBegin + pipelineLayoutRecord.object.offset;
		pipelineLayoutObject.size = pipelineLayoutRecord.object.size;

		pipelineLayout.halPipelineLayoutHandle = device.halDevice.createPipelineLayout(pipelineLayoutObject);
		pipelineLayout.nameCRC = pipelineLayoutRecord.nameCRC;

		// Check ordering of pipeline layouts.
		XAssert(prevNameCRC < pipelineLayout.nameCRC);
		prevNameCRC = pipelineLayout.nameCRC;
	}

	prevNameCRC = 0;

	for (uint16 pipelineIndex = 0; pipelineIndex < packHeader.pipelineCount; pipelineIndex++)
	{
		const PipelineRecord& pipelineRecord = pipelineRecords[pipelineIndex];
		Pipeline& pipeline = pipelinesList[pipelineIndex];

		XEMasterAssert(pipelineRecord.pipelineLayoutIndex < pipelineLayoutsList.getSize());
		const PipelineLayout& pipelineLayout = pipelineLayoutsList[pipelineRecord.pipelineLayoutIndex];

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

			pipeline.halPipelineHandle = device.halDevice.createGraphicsPipeline(
				pipelineLayout.halPipelineLayoutHandle,
				graphicsBaseObject, bytecodeObjects, bytecodeObjectCount,
				HAL::RasterizerDesc {}, HAL::DepthStencilDesc {}, HAL::BlendDesc {});
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

			pipeline.halPipelineHandle = device.halDevice.createComputePipeline(
				pipelineLayout.halPipelineLayoutHandle, computeShaderBytecodeObject);
		}

		pipeline.nameCRC = pipelineRecord.nameCRC;

		// Check ordering of pipelines.
		XAssert(prevNameCRC < pipeline.nameCRC);
		prevNameCRC = pipeline.nameCRC;
	}

	SystemHeapAllocator::Release(packData);
}

HAL::PipelineLayoutHandle ShadersLoader::getPipelineLayout(uint64 pipelineLayoutNameCRC) const
{
	return HAL::ZeroPipelineLayoutHandle;
}

HAL::PipelineHandle ShadersLoader::getPipeline(uint64 pipelineNameCRC) const
{
	return HAL::ZeroPipelineHandle;
}
