#include <XLib.CRC.h>

#include "XEngine.Gfx.HAL.BlobFormat.h"

using namespace XLib;
using namespace XEngine::Gfx::HAL;
using namespace XEngine::Gfx::HAL::BlobFormat;
using namespace XEngine::Gfx::HAL::BlobFormat::Data;

static inline void FillGenericBlobHeader(void* blobData, uint32 blobSize, uint16 specificSignature, uint16 version)
{
	GenericBlobHeader& header = *(GenericBlobHeader*)blobData;
	header.genericSignature = GenericBlobSignature;
	header.size = blobSize;
	header.checksum = 0;
	header.specificSignature = specificSignature;
	header.version = version;

	const uint32 crc = CRC32::Compute(blobData, blobSize);
	header.checksum = crc;
}

static inline bool ValidateGenericBlobHeader(const void* blobData, uint32 blobSize, uint16 specificSignature, uint16 version)
{
	if (blobSize < sizeof(GenericBlobHeader))
		return false;

	const GenericBlobHeader& header = *(const GenericBlobHeader*)blobData;
	if (header.genericSignature != GenericBlobSignature)
		return false;
	if (header.size != blobSize)
		return false;
	if (header.specificSignature != specificSignature)
		return false;
	if (header.version != version)
		return false;
	return true;
}


// Descriptor set layout blob //////////////////////////////////////////////////////////////////////

void DescriptorSetLayoutBlobWriter::setup(const DescriptorSetLayoutBlobInfo& blobInfo)
{
	XAssert(blobSize == 0);
	this->blobInfo = blobInfo;

	blobSize =
		sizeof(GenericBlobHeader) +
		sizeof(DescriptorSetLayoutBlobSubHeader) +
		sizeof(DescriptorSetBindingRecord) * blobInfo.bindingCount;
}

void DescriptorSetLayoutBlobWriter::setBlobMemory(void* memory, uint32 memorySize)
{
	XAssert(blobSize > 0 && !blobMemory);
	XAssert(blobSize == memorySize);

	blobMemory = memory;
	memorySet(blobMemory, 0, blobSize);

	{
		uint32 offsetAccum = 0;
		offsetAccum += sizeof(GenericBlobHeader);

		const uint32 subHeaderOffset = offsetAccum;
		offsetAccum += sizeof(DescriptorSetLayoutBlobSubHeader);

		const uint32 bindingRecordsOffset = offsetAccum;
		offsetAccum += sizeof(DescriptorSetBindingRecord) * blobInfo.bindingCount;

		XAssert(offsetAccum == blobSize);

		subHeader = (DescriptorSetLayoutBlobSubHeader*)(uintptr(blobMemory) + subHeaderOffset);
		bindingRecords = (DescriptorSetBindingRecord*)(uintptr(blobMemory) + bindingRecordsOffset);
	}
}

void DescriptorSetLayoutBlobWriter::writeBindingInfo(uint16 bindingIndex, const DescriptorSetBindingInfo& bindingInfo)
{
	XAssert(bindingRecords);
	XAssert(bindingIndex < blobInfo.bindingCount);

	DescriptorSetBindingRecord& record = bindingRecords[bindingIndex];
	record.nameXSH0 = uint32(bindingInfo.nameXSH);
	record.nameXSH1 = uint32(bindingInfo.nameXSH >> 32);
	record.descriptorCount = bindingInfo.descriptorCount;
	record.descriptorType = bindingInfo.descriptorType;
}

void DescriptorSetLayoutBlobWriter::finalize()
{
	XAssert(blobMemory);

	subHeader->hash = blobInfo.hash;
	subHeader->bindingCount = blobInfo.bindingCount;

	FillGenericBlobHeader(blobMemory, blobSize, DescriptorSetLayoutBlobSignature, 0);
	memorySet(this, 0, sizeof(*this));
}


bool DescriptorSetLayoutBlobReader::open(const void* data, uint32 size)
{
	XAssert(!subHeader);

	if (!ValidateGenericBlobHeader(data, size, DescriptorSetLayoutBlobSignature, 0))
		return false;

	{
		uint32 offsetAccum = 0;
		offsetAccum += sizeof(GenericBlobHeader);

		const uint32 subHeaderOffset = offsetAccum;
		offsetAccum += sizeof(DescriptorSetLayoutBlobSubHeader);

		if (size < offsetAccum)
			return false;
		const DescriptorSetLayoutBlobSubHeader* subHeader = (DescriptorSetLayoutBlobSubHeader*)(uintptr(data) + subHeaderOffset);

		const uint32 bindingRecordsOffset = offsetAccum;
		offsetAccum += sizeof(DescriptorSetBindingRecord) * subHeader->bindingCount;

		if (offsetAccum != size)
			return false;

		this->subHeader = subHeader;
		this->bindingRecords = (DescriptorSetBindingRecord*)(uintptr(data) + bindingRecordsOffset);
	}

	return true;
}

DescriptorSetLayoutBlobInfo DescriptorSetLayoutBlobReader::getBlobInfo() const
{
	XAssert(subHeader);
	DescriptorSetLayoutBlobInfo blobInfo = {};
	blobInfo.bindingCount = subHeader->bindingCount;
	blobInfo.hash = subHeader->hash;
	return blobInfo;
}

DescriptorSetBindingInfo DescriptorSetLayoutBlobReader::getBindingInfo(uint32 bindingIndex) const
{
	XAssert(bindingRecords);
	XAssert(bindingIndex < subHeader->bindingCount);
	const DescriptorSetBindingRecord& record = bindingRecords[bindingIndex];

	DescriptorSetBindingInfo bindingInfo = {};
	bindingInfo.nameXSH = uint64(record.nameXSH0) | (uint64(record.nameXSH1) << 32);
	bindingInfo.descriptorCount = record.descriptorCount;
	bindingInfo.descriptorType = record.descriptorType;
	return bindingInfo;
}


// Pipeline binding layout blob ////////////////////////////////////////////////////////////////////

void PipelineBindingLayoutBlobWriter::setup(const PipelineBindingLayoutBlobInfo& blobInfo)
{
	XAssert(blobSize == 0);
	this->blobInfo = blobInfo;

	blobSize =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineBindingLayoutBlobSubHeader) +
		sizeof(PipelineBindingRecord) * blobInfo.bindingCount +
		blobInfo.platformDataSize;
}

void PipelineBindingLayoutBlobWriter::setBlobMemory(void* memory, uint32 memorySize)
{
	XAssert(blobSize > 0 && !blobMemory);
	XAssert(blobSize == memorySize);

	blobMemory = memory;
	memorySet(blobMemory, 0, blobSize);

	{
		uint32 offsetAccum = 0;
		offsetAccum += sizeof(GenericBlobHeader);

		const uint32 subHeaderOffset = offsetAccum;
		offsetAccum += sizeof(PipelineBindingLayoutBlobSubHeader);

		const uint32 bindingRecordsOffset = offsetAccum;
		offsetAccum += sizeof(PipelineBindingRecord) * blobInfo.bindingCount;

		const uint32 platformDataOffset = offsetAccum;
		offsetAccum += blobInfo.platformDataSize;

		XAssert(offsetAccum == blobSize);

		subHeader = (PipelineBindingLayoutBlobSubHeader*)(uintptr(blobMemory) + subHeaderOffset);
		bindingRecords = (PipelineBindingRecord*)(uintptr(blobMemory) + bindingRecordsOffset);
		platformData = (void*)(uintptr(blobMemory) + platformDataOffset);
	}
}

void PipelineBindingLayoutBlobWriter::writeBindingInfo(uint16 bindingIndex, const PipelineBindingInfo& bindingInfo)
{
	XAssert(bindingRecords);
	XAssert(bindingIndex < blobInfo.bindingCount);

	PipelineBindingRecord& record = bindingRecords[bindingIndex];
	record.nameXSH = bindingInfo.nameXSH;
	record.d3dRootParameterIndex = bindingInfo.d3dRootParameterIndex;
	record.type = bindingInfo.type;
	record.descriptorSetLayoutHash = bindingInfo.type == PipelineBindingType::DescriptorSet ? bindingInfo.descriptorSetLayoutHash : 0;
	record.inplaceConstantCount = bindingInfo.type == PipelineBindingType::InplaceConstants ? bindingInfo.inplaceConstantCount : 0;
}

void PipelineBindingLayoutBlobWriter::writePlatformData(const void* data, uint32 size)
{
	XAssert(platformData);
	XAssert(size == blobInfo.platformDataSize);
	memoryCopy(platformData, data, size);
}

void PipelineBindingLayoutBlobWriter::finalize()
{
	XAssert(blobMemory);

	subHeader->hash = blobInfo.hash;
	subHeader->bindingCount = blobInfo.bindingCount;
	subHeader->platformDataSize = XCheckedCastU16(blobInfo.platformDataSize);

	FillGenericBlobHeader(blobMemory, blobSize, PipelineBindingLayoutBlobSignature, 0);
	memorySet(this, 0, sizeof(*this));
}


bool PipelineBindingLayoutBlobReader::open(const void* data, uint32 size)
{
	XAssert(!subHeader);

	if (!ValidateGenericBlobHeader(data, size, PipelineBindingLayoutBlobSignature, 0))
		return false;

	{
		uint32 offsetAccum = 0;
		offsetAccum += sizeof(GenericBlobHeader);

		const uint32 subHeaderOffset = offsetAccum;
		offsetAccum += sizeof(PipelineBindingLayoutBlobSubHeader);

		if (size < offsetAccum)
			return false;
		const PipelineBindingLayoutBlobSubHeader* subHeader = (const PipelineBindingLayoutBlobSubHeader*)(uintptr(data) + subHeaderOffset);

		const uint32 bindingRecordsOffset = offsetAccum;
		offsetAccum += sizeof(PipelineBindingRecord) * subHeader->bindingCount;

		const uint32 platformDataOffset = offsetAccum;
		offsetAccum += subHeader->platformDataSize;

		if (offsetAccum != size)
			return false;

		this->subHeader = subHeader;
		this->bindingRecords = (const PipelineBindingRecord*)(uintptr(data) + bindingRecordsOffset);
		this->platformData = (const void*)(uintptr(data) + platformDataOffset);
	}

	return true;
}

PipelineBindingLayoutBlobInfo PipelineBindingLayoutBlobReader::getBlobInfo() const
{
	XAssert(subHeader);
	PipelineBindingLayoutBlobInfo blobInfo = {};
	blobInfo.bindingCount = subHeader->bindingCount;
	blobInfo.platformDataSize = subHeader->platformDataSize;
	blobInfo.hash = subHeader->hash;
	return blobInfo;
}

PipelineBindingInfo PipelineBindingLayoutBlobReader::getBindingInfo(uint16 bindingIndex) const
{
	XAssert(bindingRecords);
	XAssert(bindingIndex < subHeader->bindingCount);
	const PipelineBindingRecord& record = bindingRecords[bindingIndex];

	PipelineBindingInfo bindingInfo = {};
	bindingInfo.nameXSH = record.nameXSH;
	bindingInfo.d3dRootParameterIndex = record.d3dRootParameterIndex;
	bindingInfo.type = record.type;
	if (record.type == PipelineBindingType::DescriptorSet)
		bindingInfo.descriptorSetLayoutHash = record.descriptorSetLayoutHash;
	else if (record.type == PipelineBindingType::InplaceConstants)
		bindingInfo.inplaceConstantCount = record.inplaceConstantCount;
	return bindingInfo;
}


// Shader blob /////////////////////////////////////////////////////////////////////////////////////

void ShaderBlobWriter::setup(const ShaderBlobInfo& blobInfo)
{
	XAssert(blobSize == 0);
	this->blobInfo = blobInfo;

	blobSize =
		sizeof(GenericBlobHeader) +
		sizeof(ShaderBlobSubHeader) +
		blobInfo.bytecodeSize;
}

void ShaderBlobWriter::setBlobMemory(void* memory, uint32 memorySize)
{
	XAssert(blobSize > 0 && !blobMemory);
	XAssert(blobSize == memorySize);

	blobMemory = memory;
	memorySet(blobMemory, 0, blobSize);

	{
		uint32 offsetAccum = 0;
		offsetAccum += sizeof(GenericBlobHeader);

		const uint32 subHeaderOffset = offsetAccum;
		offsetAccum += sizeof(ShaderBlobSubHeader);

		const uint32 bytecodeOffset = offsetAccum;
		offsetAccum += blobInfo.bytecodeSize;

		XAssert(offsetAccum == blobSize);

		subHeader = (ShaderBlobSubHeader*)(uintptr(blobMemory) + subHeaderOffset);
		bytecode = (void*)(uintptr(blobMemory) + bytecodeOffset);
	}
}

void ShaderBlobWriter::writeBytecode(const void* data, uint32 size)
{
	XAssert(bytecode);
	XAssert(size == blobInfo.bytecodeSize);
	memoryCopy(bytecode, data, size);
}

void ShaderBlobWriter::finalize()
{
	XAssert(blobMemory);

	subHeader->pipelineBindingLayoutHash = blobInfo.pipelineBindingLayoutHash;
	subHeader->bytecodeSizeLo16 = uint16(blobInfo.bytecodeSize);
	subHeader->bytecodeSizeHi8 = XCheckedCastU8(blobInfo.bytecodeSize >> 16);
	subHeader->shaderType = blobInfo.shaderType;

	FillGenericBlobHeader(blobMemory, blobSize, ShaderBlobSignature, 0);
	memorySet(this, 0, sizeof(*this));
}


bool ShaderBlobReader::open(const void* data, uint32 size)
{
	XAssert(!subHeader);

	if (!ValidateGenericBlobHeader(data, size, ShaderBlobSignature, 0))
		return false;

	{
		uint32 offsetAccum = 0;
		offsetAccum += sizeof(GenericBlobHeader);

		const uint32 subHeaderOffset = offsetAccum;
		offsetAccum += sizeof(ShaderBlobSubHeader);

		if (size < offsetAccum)
			return false;
		const ShaderBlobSubHeader* subHeader = (ShaderBlobSubHeader*)(uintptr(data) + subHeaderOffset);

		const uint32 bytecodeSize = uint32(subHeader->bytecodeSizeLo16) | (uint32(subHeader->bytecodeSizeHi8) << 16);

		const uint32 bytecodeOffset = offsetAccum;
		offsetAccum += bytecodeSize;

		if (offsetAccum != size)
			return false;

		this->subHeader = subHeader;
		this->bytecode = (const void*)(uintptr(data) + bytecodeOffset);
	}

	return true;
}

ShaderBlobInfo ShaderBlobReader::getBlobInfo() const
{
	XAssert(subHeader);
	ShaderBlobInfo blobInfo = {};
	blobInfo.pipelineBindingLayoutHash = subHeader->pipelineBindingLayoutHash;
	blobInfo.bytecodeSize = uint32(subHeader->bytecodeSizeLo16) | (uint32(subHeader->bytecodeSizeHi8) << 16);
	blobInfo.shaderType = subHeader->shaderType;
	return blobInfo;
}
