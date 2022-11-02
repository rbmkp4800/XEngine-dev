#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Render.Shaders.PackFormat.h>

#include "XEngine.Render.h"

#include "XEngine.Render.ShaderLibrary.h"

using namespace XLib;
using namespace XEngine::Render;

// TODO: Implement proper search for all getters.

struct ShaderLibrary::DescriptorSetLayout
{
	uint64 nameXSH;
	HAL::DescriptorSetLayoutHandle halDescriptorSetLayout;
};

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

		descriptorSetLayoutTable = nullptr;
		pipelineLayoutTable = nullptr;
		pipelineTable = nullptr;
		descriptorSetLayoutCount = 0;
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
	XEMasterAssert(packHeader.bytecodeBlobCount > 0);

	const uint32 blobsDataOffset = sizeof(PackHeader) +
		sizeof(PipelineLayoutRecord) * packHeader.pipelineLayoutCount +
		sizeof(PipelineRecord) * packHeader.pipelineCount +
		sizeof(BlobRecord) * packHeader.bytecodeBlobCount;
	XEMasterAssert(packHeader.blobsDataOffset == blobsDataOffset);
	XEMasterAssert(packHeader.blobsDataOffset < packFileSize);

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
	const BlobRecord* bytecodeBlobRecords = (BlobRecord*)(pipelineRecords + packHeader.pipelineCount);
	const byte* blobsDataBegin = (byte*)(bytecodeBlobRecords + packHeader.bytecodeBlobCount);
	XEAssert(uintptr(blobsDataBegin) == uintptr(packData) + blobsDataOffset);

	byte* packDataEnd = packData + packFileSize;
	XEAssert(blobsDataBegin < packDataEnd);
	const uint64 blobsDataSize = packDataEnd - blobsDataBegin;

	uint64 prevNameXSH = 0;

	for (uint16 pipelineLayoutIndex = 0; pipelineLayoutIndex < packHeader.pipelineLayoutCount; pipelineLayoutIndex++)
	{
		const PipelineLayoutRecord& pipelineLayoutRecord = pipelineLayoutRecords[pipelineLayoutIndex];
		PipelineLayout& pipelineLayout = pipelineLayoutTable[pipelineLayoutIndex];

		HAL::BlobDataView pipelineLayoutBlob = {};
		pipelineLayoutBlob.data = blobsDataBegin + pipelineLayoutRecord.blob.offset;
		pipelineLayoutBlob.size = pipelineLayoutRecord.blob.size;

		pipelineLayout.halPipelineLayout = halDevice.createPipelineLayout(pipelineLayoutBlob);
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
				pipelineRecord.graphicsBaseBlob.offset + pipelineRecord.graphicsBaseBlob.size <= blobsDataSize);

			HAL::BlobDataView graphicsBaseBlob = {};
			graphicsBaseBlob.data = blobsDataBegin + pipelineRecord.graphicsBaseBlob.offset;
			graphicsBaseBlob.size = pipelineRecord.graphicsBaseBlob.size;

			HAL::BlobDataView bytecodeBlobs[HAL::MaxGraphicsPipelineBytecodeBlobCount] = {};
			uint8 bytecodeBlobCount = 0;

			// Fill `bytecodeBlobs`.
			bool invalidBytecodeBlobIndexReached = false;
			for (uint8 i = 0; i < countof(pipelineRecord.bytecodeBlobIndices); i++)
			{
				const uint16 bytecodeBlobIndex = pipelineRecord.bytecodeBlobIndices[i];
				if (bytecodeBlobIndex == uint16(-1))
				{
					invalidBytecodeBlobIndexReached = true;
					continue;
				}

				XEMasterAssert(!invalidBytecodeBlobIndexReached);
				XEMasterAssert(i < HAL::MaxGraphicsPipelineBytecodeBlobCount);
				XEMasterAssert(bytecodeBlobIndex < packHeader.bytecodeBlobCount);
				const BlobRecord& bytecodeBlobRecord = bytecodeBlobRecords[bytecodeBlobIndex];

				XEMasterAssert(bytecodeBlobRecord.offset + bytecodeBlobRecord.size <= blobsDataSize);

				bytecodeBlobs[i].data = blobsDataBegin + bytecodeBlobRecord.offset;
				bytecodeBlobs[i].size = bytecodeBlobRecord.size;
				bytecodeBlobCount++;
			}

			XEMasterAssert(bytecodeBlobCount > 0);

			pipeline.halPipeline = halDevice.createGraphicsPipeline(
				pipelineLayout.halPipelineLayout,
				graphicsBaseBlob, bytecodeBlobs, bytecodeBlobCount);
		}
		else
		{
			// TODO: Asserts everywhere.
			// TODO: Check that all bytecode blob indicies after first are invalid.
			const uint16 computeShaderBlobIndex = pipelineRecord.bytecodeBlobIndices[0];
			XEMasterAssert(computeShaderBlobIndex < packHeader.bytecodeBlobCount);
			const BlobRecord& computeShaderBlobRecord = bytecodeBlobRecords[computeShaderBlobIndex];

			XEMasterAssert(computeShaderBlobRecord.offset + computeShaderBlobRecord.size <= blobsDataSize);

			HAL::BlobDataView computeShaderBlob = {};
			computeShaderBlob.data = blobsDataBegin + computeShaderBlobRecord.offset;
			computeShaderBlob.size = computeShaderBlobRecord.size;

			pipeline.halPipeline = halDevice.createComputePipeline(
				pipelineLayout.halPipelineLayout, computeShaderBlob);
		}

		pipeline.nameXSH = pipelineRecord.nameXSH;

		// Check ordering of pipelines.
		XEMasterAssert(prevNameXSH < pipeline.nameXSH);
		prevNameXSH = pipeline.nameXSH;
	}

	SystemHeapAllocator::Release(packData);
}

HAL::DescriptorSetLayoutHandle ShaderLibrary::getDescriptorSetLayout(uint64 nameXSH) const
{
	for (uint32 i = 0; i < descriptorSetLayoutCount; i++)
	{
		if (descriptorSetLayoutTable[i].nameXSH == nameXSH)
			return descriptorSetLayoutTable[i].halDescriptorSetLayout;
	}
	XEMasterAssertUnreachableCode();
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
