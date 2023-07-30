#include <XLib.System.File.h>
#include <XLib.SystemHeapAllocator.h>

#include <XEngine.Gfx.ShaderLibraryFormat.h>

#include "XEngine.Gfx.ShaderLibrary.h"

using namespace XLib;
using namespace XEngine::Gfx;

XTODO("Use hash maps instead of linear search");

static inline uint64 U64From2xU32(uint32 lo, uint32 hi) { return uint64(lo) | (uint64(hi) << 32); }

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
	if (memoryBlock)
	{
		// TODO: Do this properly.
		SystemHeapAllocator::Release(memoryBlock);
		memoryBlock = nullptr;

		descriptorSetLayoutTable = nullptr;
		pipelineLayoutTable = nullptr;
		pipelineTable = nullptr;
		descriptorSetLayoutCount = 0;
		pipelineLayoutCount = 0;
		pipelineCount = 0;
	}
}

void ShaderLibrary::load(const char* libraryFilePath, HAL::Device& halDevice)
{
	// TODO: Refactor this method.

	File libraryFile;
	libraryFile.open(libraryFilePath, FileAccessMode::Read, FileOpenMode::OpenExisting);
	XEMasterAssert(libraryFile.isInitialized());

	const uint64 libraryFileSize = libraryFile.getSize();
	XEMasterAssert(libraryFileSize > sizeof(ShaderLibraryFormat::LibraryHeader));

	ShaderLibraryFormat::LibraryHeader libraryHeader = {};
	libraryFile.read(libraryHeader);
	XEMasterAssert(libraryHeader.signature == ShaderLibraryFormat::LibrarySignature);
	XEMasterAssert(libraryHeader.version == ShaderLibraryFormat::LibraryCurrentVersion);
	XEMasterAssert(libraryHeader.pipelineLayoutCount > 0);
	XEMasterAssert(libraryHeader.pipelineCount > 0);
	XEMasterAssert(libraryHeader.bytecodeBlobCount > 0);

	const uint32 descriptorSetLayoutRecordsOffset = sizeof(ShaderLibraryFormat::LibraryHeader);
	const uint32 pipelineLayoutRecordsOffset = descriptorSetLayoutRecordsOffset + sizeof(ShaderLibraryFormat::DescriptorSetLayoutRecord) * libraryHeader.descriptorSetLayoutCount;
	const uint32 pipelineRecordsOffset = pipelineLayoutRecordsOffset + sizeof(ShaderLibraryFormat::PipelineLayoutRecord) * libraryHeader.pipelineLayoutCount;
	const uint32 bytecodeBlobRecordsOffset = pipelineRecordsOffset + sizeof(ShaderLibraryFormat::PipelineRecord) * libraryHeader.pipelineCount;
	const uint32 blobsDataOffset = bytecodeBlobRecordsOffset + sizeof(ShaderLibraryFormat::BlobRecord) * libraryHeader.bytecodeBlobCount;
	XEMasterAssert(libraryHeader.blobsDataOffset == blobsDataOffset);
	XEMasterAssert(libraryHeader.blobsDataOffset < libraryFileSize);

	descriptorSetLayoutCount = libraryHeader.descriptorSetLayoutCount;
	pipelineLayoutCount = libraryHeader.pipelineLayoutCount;
	pipelineCount = libraryHeader.pipelineCount;

	// Allocate tables.
	{
		// TODO: Handle this memory in proper way.
		const uintptr memoryBlockSize =
			sizeof(DescriptorSetLayout) * descriptorSetLayoutCount +
			sizeof(PipelineLayout) * pipelineLayoutCount +
			sizeof(Pipeline) * pipelineCount;
		memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
		memorySet(memoryBlock, 0, memoryBlockSize);

		descriptorSetLayoutTable = (DescriptorSetLayout*)memoryBlock;
		pipelineLayoutTable = (PipelineLayout*)(descriptorSetLayoutTable + descriptorSetLayoutCount);
		pipelineTable = (Pipeline*)(pipelineLayoutTable + pipelineLayoutCount);
		void* memoryBlockEnd = pipelineTable + pipelineCount;

		XEAssert(uintptr(memoryBlock) + memoryBlockSize == uintptr(memoryBlockEnd));
	}

	byte* libraryData = (byte*)SystemHeapAllocator::Allocate(libraryFileSize);

	memoryCopy(libraryData, &libraryHeader, sizeof(ShaderLibraryFormat::LibraryHeader)); // Header is already read.
	libraryFile.read(libraryData + sizeof(ShaderLibraryFormat::LibraryHeader), libraryFileSize - sizeof(ShaderLibraryFormat::LibraryHeader));

	auto descriptorSetLayoutRecords = (const ShaderLibraryFormat::DescriptorSetLayoutRecord*)(libraryData + descriptorSetLayoutRecordsOffset);
	auto pipelineLayoutRecords = (const ShaderLibraryFormat::PipelineLayoutRecord*)(libraryData + pipelineLayoutRecordsOffset);
	auto pipelineRecords = (const ShaderLibraryFormat::PipelineRecord*)(libraryData + pipelineRecordsOffset);
	auto bytecodeBlobRecords = (const ShaderLibraryFormat::BlobRecord*)(libraryData + bytecodeBlobRecordsOffset);
	auto blobsDataBegin = (const byte*)(libraryData + blobsDataOffset);

	byte* libraryDataEnd = libraryData + libraryFileSize;
	XEAssert(blobsDataBegin < libraryDataEnd);
	const uint64 blobsDataSize = libraryDataEnd - blobsDataBegin;

	uint64 prevNameXSH = 0;

	for (uint32 descriptorSetLayoutIndex = 0; descriptorSetLayoutIndex < libraryHeader.descriptorSetLayoutCount; descriptorSetLayoutIndex++)
	{
		const ShaderLibraryFormat::DescriptorSetLayoutRecord& descriptorSetLayoutRecord = descriptorSetLayoutRecords[descriptorSetLayoutIndex];
		DescriptorSetLayout& descriptorSetLayout = descriptorSetLayoutTable[descriptorSetLayoutIndex];

		HAL::BlobDataView descriptorSetLayoutBlob = {};
		descriptorSetLayoutBlob.data = blobsDataBegin + descriptorSetLayoutRecord.blob.offset;
		descriptorSetLayoutBlob.size = descriptorSetLayoutRecord.blob.size;

		descriptorSetLayout.halDescriptorSetLayout = halDevice.createDescriptorSetLayout(descriptorSetLayoutBlob);
		descriptorSetLayout.nameXSH = U64From2xU32(descriptorSetLayoutRecord.nameXSH0, descriptorSetLayoutRecord.nameXSH1);

		XEMasterAssert(descriptorSetLayout.nameXSH);
		// TODO: Uncomment when proper objects ordering is implemented.
		//XEMasterAssert(prevNameXSH < descriptorSetLayout.nameXSH); // Check order.
		prevNameXSH = descriptorSetLayout.nameXSH;
	}

	prevNameXSH = 0;

	for (uint32 pipelineLayoutIndex = 0; pipelineLayoutIndex < libraryHeader.pipelineLayoutCount; pipelineLayoutIndex++)
	{
		const ShaderLibraryFormat::PipelineLayoutRecord& pipelineLayoutRecord = pipelineLayoutRecords[pipelineLayoutIndex];
		PipelineLayout& pipelineLayout = pipelineLayoutTable[pipelineLayoutIndex];

		HAL::BlobDataView pipelineLayoutBlob = {};
		pipelineLayoutBlob.data = blobsDataBegin + pipelineLayoutRecord.blob.offset;
		pipelineLayoutBlob.size = pipelineLayoutRecord.blob.size;

		pipelineLayout.halPipelineLayout = halDevice.createPipelineLayout(pipelineLayoutBlob);
		pipelineLayout.nameXSH = U64From2xU32(pipelineLayoutRecord.nameXSH0, pipelineLayoutRecord.nameXSH1);

		XEMasterAssert(pipelineLayout.nameXSH);
		// TODO: Uncomment when proper objects ordering is implemented.
		//XEMasterAssert(prevNameXSH < pipelineLayout.nameXSH); // Check order.
		prevNameXSH = pipelineLayout.nameXSH;
	}

	prevNameXSH = 0;

	for (uint32 pipelineIndex = 0; pipelineIndex < libraryHeader.pipelineCount; pipelineIndex++)
	{
		const ShaderLibraryFormat::PipelineRecord& pipelineRecord = pipelineRecords[pipelineIndex];
		Pipeline& pipeline = pipelineTable[pipelineIndex];

		const uint64 pipelineLayoutNameXSH = U64From2xU32(pipelineRecord.pipelineLayoutNameXSH0, pipelineRecord.pipelineLayoutNameXSH1);
		const PipelineLayout* pipelineLayout = nullptr;
		for (uint32 i = 0; i < pipelineLayoutCount; i++)
		{
			if (pipelineLayoutTable[i].nameXSH == pipelineLayoutNameXSH)
			{
				pipelineLayout = &pipelineLayoutTable[i];
				break;
			}
		}
		XEMasterAssert(pipelineLayout);

		const bool isGraphics = pipelineRecord.graphicsStateBlob.size > 0;
		if (isGraphics)
		{
			XEMasterAssert(pipelineRecord.graphicsStateBlob.offset + pipelineRecord.graphicsStateBlob.size <= blobsDataSize);

			HAL::GraphicsPipelineBlobs graphicsBlobs = {};
			graphicsBlobs.state.data = blobsDataBegin + pipelineRecord.graphicsStateBlob.offset;
			graphicsBlobs.state.size = pipelineRecord.graphicsStateBlob.size;

			if (pipelineRecord.vsBytecodeBlobIndex != uint16(-1))
			{
				XEMasterAssert(pipelineRecord.vsBytecodeBlobIndex < libraryHeader.bytecodeBlobCount);
				const ShaderLibraryFormat::BlobRecord& bytecodeBlobRecord = bytecodeBlobRecords[pipelineRecord.vsBytecodeBlobIndex];
				XEMasterAssert(bytecodeBlobRecord.offset + bytecodeBlobRecord.size <= blobsDataSize);
				graphicsBlobs.vs.data = blobsDataBegin + bytecodeBlobRecord.offset;
				graphicsBlobs.vs.size = bytecodeBlobRecord.size;
			}
			if (pipelineRecord.asBytecodeBlobIndex != uint16(-1))
			{
				XEMasterAssert(pipelineRecord.asBytecodeBlobIndex < libraryHeader.bytecodeBlobCount);
				const ShaderLibraryFormat::BlobRecord& bytecodeBlobRecord = bytecodeBlobRecords[pipelineRecord.asBytecodeBlobIndex];
				XEMasterAssert(bytecodeBlobRecord.offset + bytecodeBlobRecord.size <= blobsDataSize);
				graphicsBlobs.as.data = blobsDataBegin + bytecodeBlobRecord.offset;
				graphicsBlobs.as.size = bytecodeBlobRecord.size;
			}
			if (pipelineRecord.msBytecodeBlobIndex != uint16(-1))
			{
				XEMasterAssert(pipelineRecord.msBytecodeBlobIndex < libraryHeader.bytecodeBlobCount);
				const ShaderLibraryFormat::BlobRecord& bytecodeBlobRecord = bytecodeBlobRecords[pipelineRecord.msBytecodeBlobIndex];
				XEMasterAssert(bytecodeBlobRecord.offset + bytecodeBlobRecord.size <= blobsDataSize);
				graphicsBlobs.ms.data = blobsDataBegin + bytecodeBlobRecord.offset;
				graphicsBlobs.ms.size = bytecodeBlobRecord.size;
			}
			if (pipelineRecord.psORcsBytecodeBlobIndex != uint16(-1))
			{
				XEMasterAssert(pipelineRecord.psORcsBytecodeBlobIndex < libraryHeader.bytecodeBlobCount);
				const ShaderLibraryFormat::BlobRecord& bytecodeBlobRecord = bytecodeBlobRecords[pipelineRecord.psORcsBytecodeBlobIndex];
				XEMasterAssert(bytecodeBlobRecord.offset + bytecodeBlobRecord.size <= blobsDataSize);
				graphicsBlobs.ps.data = blobsDataBegin + bytecodeBlobRecord.offset;
				graphicsBlobs.ps.size = bytecodeBlobRecord.size;
			}

			pipeline.halPipeline = halDevice.createGraphicsPipeline(pipelineLayout->halPipelineLayout, graphicsBlobs);
		}
		else
		{
			XEMasterAssert(pipelineRecord.psORcsBytecodeBlobIndex != uint16(-1));
			XEMasterAssert(pipelineRecord.psORcsBytecodeBlobIndex < libraryHeader.bytecodeBlobCount);
			const ShaderLibraryFormat::BlobRecord& bytecodeBlobRecord = bytecodeBlobRecords[pipelineRecord.psORcsBytecodeBlobIndex];
			XEMasterAssert(bytecodeBlobRecord.offset + bytecodeBlobRecord.size <= blobsDataSize);

			HAL::BlobDataView csBlob = {};
			csBlob.data = blobsDataBegin + bytecodeBlobRecord.offset;
			csBlob.size = bytecodeBlobRecord.size;

			pipeline.halPipeline = halDevice.createComputePipeline(pipelineLayout->halPipelineLayout, csBlob);
		}

		pipeline.nameXSH = U64From2xU32(pipelineRecord.nameXSH0, pipelineRecord.nameXSH1);

		XEMasterAssert(pipeline.nameXSH);
		// TODO: Uncomment when proper objects ordering is implemented.
		//XEMasterAssert(prevNameXSH < pipeline.nameXSH); // Check order.
		prevNameXSH = pipeline.nameXSH;
	}

	SystemHeapAllocator::Release(libraryData);
}

HAL::DescriptorSetLayoutHandle ShaderLibrary::getDescriptorSetLayout(uint64 nameXSH) const
{
	for (uint32 i = 0; i < descriptorSetLayoutCount; i++)
	{
		if (descriptorSetLayoutTable[i].nameXSH == nameXSH)
			return descriptorSetLayoutTable[i].halDescriptorSetLayout;
	}

	XEMasterAssertUnreachableCode();
	return HAL::DescriptorSetLayoutHandle::Zero;
}

HAL::PipelineLayoutHandle ShaderLibrary::getPipelineLayout(uint64 nameXSH) const
{
	for (uint32 i = 0; i < pipelineLayoutCount; i++)
	{
		if (pipelineLayoutTable[i].nameXSH == nameXSH)
			return pipelineLayoutTable[i].halPipelineLayout;
	}

	XEMasterAssertUnreachableCode();
	return HAL::PipelineLayoutHandle::Zero;
}

HAL::PipelineHandle ShaderLibrary::getPipeline(uint64 nameXSH) const
{
	for (uint32 i = 0; i < pipelineCount; i++)
	{
		if (pipelineTable[i].nameXSH == nameXSH)
			return pipelineTable[i].halPipeline;
	}

	XEMasterAssertUnreachableCode();
	return HAL::PipelineHandle::Zero;
}
