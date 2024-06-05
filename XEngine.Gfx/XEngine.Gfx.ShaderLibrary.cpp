#include <XLib.Allocation.h>
#include <XLib.System.File.h>

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

struct ShaderLibrary::Shader
{
	uint64 nameXSH;
	HAL::ShaderHandle halShader;
};

ShaderLibrary::~ShaderLibrary()
{
	if (memoryBlock)
	{
		// TODO: Do this properly.
		SystemHeapAllocator::Release(memoryBlock);
		memorySet(this, 0, sizeof(*this));
	}
}

void ShaderLibrary::load(const char* libraryFilePath, HAL::Device& halDevice)
{
	// TODO: Refactor this method.

	File libraryFile;
	libraryFile.open(libraryFilePath, FileAccessMode::Read, FileOpenMode::OpenExisting);
	XEMasterAssert(libraryFile.isOpen());

	const uint64 libraryFileSize = libraryFile.getSize();
	XEMasterAssert(libraryFileSize != uint64(-1));
	XEMasterAssert(libraryFileSize > sizeof(ShaderLibraryFormat::LibraryHeader));

	ShaderLibraryFormat::LibraryHeader libraryHeader = {};
	libraryFile.read(&libraryHeader, sizeof(libraryHeader));
	XEMasterAssert(libraryHeader.signature == ShaderLibraryFormat::LibrarySignature);
	XEMasterAssert(libraryHeader.version == ShaderLibraryFormat::LibraryCurrentVersion);
	XEMasterAssert(libraryHeader.pipelineLayoutCount > 0);
	XEMasterAssert(libraryHeader.shaderCount > 0);

	uint32 fileOffsetAccum = 0;
	fileOffsetAccum += sizeof(ShaderLibraryFormat::LibraryHeader);

	const uint32 descriptorSetLayoutRecordsFileOffset = fileOffsetAccum;
	fileOffsetAccum += sizeof(ShaderLibraryFormat::DescriptorSetLayoutRecord) * libraryHeader.descriptorSetLayoutCount;

	const uint32 pipelineLayoutRecordsFileOffset = fileOffsetAccum;
	fileOffsetAccum += sizeof(ShaderLibraryFormat::PipelineLayoutRecord) * libraryHeader.pipelineLayoutCount;

	const uint32 shaderRecordsFileOffset = fileOffsetAccum;
	fileOffsetAccum += sizeof(ShaderLibraryFormat::ShaderRecord) * libraryHeader.shaderCount;

	const uint32 blobsDataFileOffset = fileOffsetAccum;
	fileOffsetAccum += libraryHeader.blobsDataSize;

	const uint32 libraryFileSizeCheck = fileOffsetAccum;

	XEMasterAssert(libraryHeader.blobsDataOffset == blobsDataFileOffset);
	XEMasterAssert(libraryFileSizeCheck == libraryFileSize);

	descriptorSetLayoutCount = libraryHeader.descriptorSetLayoutCount;
	pipelineLayoutCount = libraryHeader.pipelineLayoutCount;
	shaderCount = libraryHeader.shaderCount;

	// Allocate tables.
	{
		uint32 memoryBlockSizeAccum = 0;

		const uint32 descriptorSetLayoutTableOffset = memoryBlockSizeAccum;
		memoryBlockSizeAccum += sizeof(DescriptorSetLayout) * descriptorSetLayoutCount;

		const uint32 pipelineLayoutTableOffset = memoryBlockSizeAccum;
		memoryBlockSizeAccum += sizeof(PipelineLayout) * pipelineLayoutCount;

		const uint32 shaderTableOffset = memoryBlockSizeAccum;
		memoryBlockSizeAccum += sizeof(Shader) * shaderCount;

		const uint32 memoryBlockSize = memoryBlockSizeAccum;
		void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
		memorySet(memoryBlock, 0, memoryBlockSize);

		descriptorSetLayoutTable =	(DescriptorSetLayout*)	(uintptr(memoryBlock) + descriptorSetLayoutTableOffset);
		pipelineLayoutTable =		(PipelineLayout*)		(uintptr(memoryBlock) + pipelineLayoutTableOffset);
		shaderTable =				(Shader*)				(uintptr(memoryBlock) + shaderTableOffset);
	}

	byte* libraryData = (byte*)SystemHeapAllocator::Allocate(libraryFileSize);

	memoryCopy(libraryData, &libraryHeader, sizeof(ShaderLibraryFormat::LibraryHeader)); // Header is already read.
	libraryFile.read(libraryData + sizeof(ShaderLibraryFormat::LibraryHeader), libraryFileSize - sizeof(ShaderLibraryFormat::LibraryHeader));

	auto descriptorSetLayoutRecords	= (const ShaderLibraryFormat::DescriptorSetLayoutRecord*)	(libraryData + descriptorSetLayoutRecordsFileOffset);
	auto pipelineLayoutRecords		= (const ShaderLibraryFormat::PipelineLayoutRecord*)		(libraryData + pipelineLayoutRecordsFileOffset);
	auto pipelineRecords			= (const ShaderLibraryFormat::ShaderRecord*)				(libraryData + shaderRecordsFileOffset);
	auto blobsDataBegin				= (const byte*)												(libraryData + blobsDataFileOffset);

	uint64 prevNameXSH = 0;

	for (uint32 descriptorSetLayoutIndex = 0; descriptorSetLayoutIndex < libraryHeader.descriptorSetLayoutCount; descriptorSetLayoutIndex++)
	{
		const ShaderLibraryFormat::DescriptorSetLayoutRecord& descriptorSetLayoutRecord = descriptorSetLayoutRecords[descriptorSetLayoutIndex];
		DescriptorSetLayout& descriptorSetLayout = descriptorSetLayoutTable[descriptorSetLayoutIndex];

		const void* blobData = blobsDataBegin + descriptorSetLayoutRecord.blobOffset;
		const uint32 blobSize = descriptorSetLayoutRecord.blobSize;

		descriptorSetLayout.halDescriptorSetLayout = halDevice.createDescriptorSetLayout(blobData, blobSize);
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

		const void* blobData = blobsDataBegin + pipelineLayoutRecord.blobOffset;
		const uint32 blobSize = pipelineLayoutRecord.blobSize;

		pipelineLayout.halPipelineLayout = halDevice.createPipelineLayout(blobData, blobSize);
		pipelineLayout.nameXSH = U64From2xU32(pipelineLayoutRecord.nameXSH0, pipelineLayoutRecord.nameXSH1);

		XEMasterAssert(pipelineLayout.nameXSH);
		// TODO: Uncomment when proper objects ordering is implemented.
		//XEMasterAssert(prevNameXSH < pipelineLayout.nameXSH); // Check order.
		prevNameXSH = pipelineLayout.nameXSH;
	}

	prevNameXSH = 0;

	for (uint32 shaderIndex = 0; shaderIndex < libraryHeader.shaderCount; shaderIndex++)
	{
		const ShaderLibraryFormat::ShaderRecord& shaderRecord = pipelineRecords[shaderIndex];
		Shader& shader = shaderTable[shaderIndex];

		const uint64 pipelineLayoutNameXSH = U64From2xU32(shaderRecord.pipelineLayoutNameXSH0, shaderRecord.pipelineLayoutNameXSH1);
		const PipelineLayout* pipelineLayout = nullptr;
		XTODO("Use fast search here too");
		for (uint32 i = 0; i < pipelineLayoutCount; i++)
		{
			if (pipelineLayoutTable[i].nameXSH == pipelineLayoutNameXSH)
			{
				pipelineLayout = &pipelineLayoutTable[i];
				break;
			}
		}
		XEMasterAssert(pipelineLayout);

		const void* blobData = blobsDataBegin + shaderRecord.blobOffset;
		const uint32 blobSize = shaderRecord.blobSize;

		shader.halShader = halDevice.createShader(pipelineLayout->halPipelineLayout, blobData, blobSize);
		shader.nameXSH = U64From2xU32(shaderRecord.nameXSH0, shaderRecord.nameXSH1);

		XEMasterAssert(shader.nameXSH);
		// TODO: Uncomment when proper objects ordering is implemented.
		//XEMasterAssert(prevNameXSH < pipeline.nameXSH); // Check order.
		prevNameXSH = shader.nameXSH;
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

HAL::ShaderHandle ShaderLibrary::getShader(uint64 nameXSH) const
{
	for (uint32 i = 0; i < shaderCount; i++)
	{
		if (shaderTable[i].nameXSH == nameXSH)
			return shaderTable[i].halShader;
	}

	XEMasterAssertUnreachableCode();
	return HAL::ShaderHandle::Zero;
}
