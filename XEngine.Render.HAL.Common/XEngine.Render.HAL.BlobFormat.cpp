#include <XLib.CRC.h>

#include "XEngine.Render.HAL.BlobFormat.h"

using namespace XLib;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::HAL::BlobFormat;
using namespace XEngine::Render::HAL::BlobFormat::Data;

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

// DescriptorSetLayoutBlobWriter ///////////////////////////////////////////////////////////////////

void DescriptorSetLayoutBlobWriter::initialize(uint16 bindingCount)
{
	XAssert(memoryBlockSize == 0); // Not initialized.
	XAssert(bindingCount > 0);
	XAssert(bindingCount <= MaxDescriptorSetBindingCount);

	this->bindingCount = bindingCount;

	memoryBlockSize =
		sizeof(GenericBlobHeader) +
		sizeof(DescriptorSetLayoutBlobSubHeader) +
		sizeof(DescriptorSetBindingRecord) * bindingCount;
}

uint32 DescriptorSetLayoutBlobWriter::getMemoryBlockSize() const
{
	XAssert(memoryBlockSize > 0);
	return memoryBlockSize;
}

void DescriptorSetLayoutBlobWriter::setMemoryBlock(void* memoryBlock, uint32 memoryBlockSize)
{
	XAssert(!this->memoryBlock);
	XAssert(this->memoryBlockSize == memoryBlockSize && memoryBlockSize > 0);
	this->memoryBlock = memoryBlock;
	memorySet(memoryBlock, 0, memoryBlockSize);
}

void DescriptorSetLayoutBlobWriter::writeBinding(uint16 bindingIndex, const DescriptorSetBindingInfo& bindingInfo)
{
	XAssert(memoryBlock);
	XAssert(bindingIndex < bindingCount);

	const uint32 offset =
		sizeof(GenericBlobHeader) +
		sizeof(DescriptorSetLayoutBlobSubHeader) +
		sizeof(DescriptorSetBindingRecord) * bindingIndex;
	DescriptorSetBindingRecord& record = *(DescriptorSetBindingRecord*)((byte*)memoryBlock + offset);
	record = {};
	record.nameXSH0 = uint32(bindingInfo.nameXSH);
	record.nameXSH1 = uint32(bindingInfo.nameXSH >> 32);
	record.descriptorCount = bindingInfo.descriptorCount;
	record.descriptorType = bindingInfo.descriptorType;
}

void DescriptorSetLayoutBlobWriter::finalize(uint32 sourceHash)
{
	XAssert(memoryBlock);

	DescriptorSetLayoutBlobSubHeader& subHeader =
		*(DescriptorSetLayoutBlobSubHeader*)((byte*)memoryBlock + sizeof(GenericBlobHeader));
	subHeader.sourceHash = sourceHash;
	subHeader.bindingCount = bindingCount;

	FillGenericBlobHeader(memoryBlock, memoryBlockSize, DescriptorSetLayoutBlobSignature, 0);
	memoryBlock = nullptr;
}

// DescriptorSetLayoutBlobReader ///////////////////////////////////////////////////////////////////

bool DescriptorSetLayoutBlobReader::open(const void* data, uint32 size)
{
	XAssert(!this->data);

	if (!ValidateGenericBlobHeader(data, size, DescriptorSetLayoutBlobSignature, 0))
		return false;
	if (size < sizeof(GenericBlobHeader) + sizeof(DescriptorSetLayoutBlobSubHeader))
		return false;

	const DescriptorSetLayoutBlobSubHeader* subHeader =
		(const DescriptorSetLayoutBlobSubHeader*)((const byte*)data + sizeof(GenericBlobHeader));

	if (subHeader->bindingCount == 0 ||
		subHeader->bindingCount > MaxDescriptorSetBindingCount)
		return false;

	const uint32 bindingRecordsOffset =
		sizeof(GenericBlobHeader) +
		sizeof(DescriptorSetLayoutBlobSubHeader);
	const uint32 sizeCheck =
		bindingRecordsOffset +
		sizeof(DescriptorSetBindingRecord) * subHeader->bindingCount;

	if (size != sizeCheck)
		return false;

	this->data = data;
	this->size = size;
	this->subHeader = subHeader;
	this->bindingRecords = (const DescriptorSetBindingRecord*)((const byte*)data + bindingRecordsOffset);
	return true;
}

DescriptorSetBindingInfo DescriptorSetLayoutBlobReader::getBinding(uint32 bindingIndex) const
{
	XAssert(bindingIndex < subHeader->bindingCount);
	const DescriptorSetBindingRecord& record = bindingRecords[bindingIndex];

	DescriptorSetBindingInfo result = {};
	result.nameXSH = uint64(record.nameXSH0) | (uint64(record.nameXSH1) << 32);
	result.descriptorCount = record.descriptorCount;
	result.descriptorType = record.descriptorType;
	return result;
}

// PipelineLayoutBlobWriter ////////////////////////////////////////////////////////////////////////

void PipelineLayoutBlobWriter::initialize(uint16 bindingCount, uint32 platformDataSize)
{
	XAssert(memoryBlockSize == 0); // Not initialized.
	XAssert(bindingCount > 0);
	XAssert(bindingCount <= MaxDescriptorSetBindingCount);

	this->bindingCount = bindingCount;
	this->platformDataSize = platformDataSize;

	memoryBlockSize =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineLayoutBlobSubHeader) +
		sizeof(PipelineBindingRecord) * bindingCount +
		platformDataSize;
}

uint32 PipelineLayoutBlobWriter::getMemoryBlockSize() const
{
	XAssert(memoryBlockSize > 0);
	return memoryBlockSize;
}

void PipelineLayoutBlobWriter::setMemoryBlock(void* memoryBlock, uint32 memoryBlockSize)
{
	XAssert(!this->memoryBlock);
	XAssert(this->memoryBlockSize == memoryBlockSize && memoryBlockSize > 0);
	this->memoryBlock = memoryBlock;
	memorySet(memoryBlock, 0, memoryBlockSize);
}

void PipelineLayoutBlobWriter::writeBinding(uint16 bindingIndex, const PipelineBindingInfo& bindingInfo)
{
	XAssert(memoryBlock);
	XAssert(bindingIndex < bindingCount);

	const uint32 offset =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineLayoutBlobSubHeader) +
		sizeof(PipelineBindingRecord) * bindingIndex;
	PipelineBindingRecord& record = *(PipelineBindingRecord*)((byte*)memoryBlock + offset);
	record = {};
	record.nameXSH = bindingInfo.nameXSH;
	record.d3dRootParameterIndex = bindingInfo.d3dRootParameterIndex;
	record.type = bindingInfo.type;
	if (bindingInfo.type == PipelineBindingType::DescriptorSet)
		record.descriptorSetLayoutSourceHash = bindingInfo.descriptorSetLayoutSourceHash;
	if (bindingInfo.type == PipelineBindingType::InplaceConstants)
		record.inplaceConstantCount = bindingInfo.inplaceConstantCount;
}

void PipelineLayoutBlobWriter::writePlatformData(const void* platformData, uint32 platformDataSize)
{
	XAssert(memoryBlock);
	XAssert(this->platformDataSize == platformDataSize);

	const uint32 offset =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineLayoutBlobSubHeader) +
		sizeof(PipelineBindingRecord) * bindingCount;
	memoryCopy((byte*)memoryBlock + offset, platformData, platformDataSize);
}

void PipelineLayoutBlobWriter::finalize(uint32 sourceHash)
{
	XAssert(memoryBlock);

	PipelineLayoutBlobSubHeader& subHeader =
		*(PipelineLayoutBlobSubHeader*)((byte*)memoryBlock + sizeof(GenericBlobHeader));
	subHeader.sourceHash = sourceHash;
	subHeader.bindingCount = bindingCount;
	subHeader.platformDataSize = XCheckedCastU16(platformDataSize);

	FillGenericBlobHeader(memoryBlock, memoryBlockSize, PipelineLayoutBlobSignature, 0);
	memoryBlock = nullptr;
}

// PipelineLayoutBlobReader ////////////////////////////////////////////////////////////////////////

bool PipelineLayoutBlobReader::open(const void* data, uint32 size)
{
	XAssert(!this->data);

	if (!ValidateGenericBlobHeader(data, size, PipelineLayoutBlobSignature, 0))
		return false;
	if (size < sizeof(GenericBlobHeader) + sizeof(PipelineLayoutBlobSubHeader))
		return false;

	const PipelineLayoutBlobSubHeader* subHeader =
		(const PipelineLayoutBlobSubHeader*)((const byte*)data + sizeof(GenericBlobHeader));

	if (subHeader->bindingCount > MaxPipelineBindingCount)
		return false;

	const uint32 bindingRecordsOffset =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineLayoutBlobSubHeader);
	const uint32 platformDataOffset =
		bindingRecordsOffset +
		sizeof(PipelineBindingRecord) * subHeader->bindingCount;
	const uint32 sizeCheck =
		platformDataOffset +
		subHeader->platformDataSize;

	if (size != sizeCheck)
		return false;

	this->data = data;
	this->size = size;
	this->subHeader = subHeader;
	this->bindingRecords = subHeader->bindingCount > 0 ? (const PipelineBindingRecord*)((const byte*)data + bindingRecordsOffset) : nullptr;
	this->platformData = (const byte*)data + platformDataOffset;
	return true;
}

PipelineBindingInfo PipelineLayoutBlobReader::getPipelineBinding(uint16 bindingIndex) const
{
	XAssert(bindingIndex < subHeader->bindingCount);
	const PipelineBindingRecord& record = bindingRecords[bindingIndex];

	PipelineBindingInfo result = {};
	result.nameXSH = record.nameXSH;
	result.d3dRootParameterIndex = record.d3dRootParameterIndex;
	result.type = record.type;
	if (record.type == PipelineBindingType::InplaceConstants)
		result.inplaceConstantCount = record.inplaceConstantCount;
	if (record.type == PipelineBindingType::DescriptorSet)
		result.descriptorSetLayoutSourceHash = record.descriptorSetLayoutSourceHash;
	return result;
}

// GraphicsPipelineStateBlobWriter /////////////////////////////////////////////////////////////////

void GraphicsPipelineStateBlobWriter::beginInitialization()
{
	XAssert(memoryBlockSize == 0);	// Not initialized.
	XAssert(!initializationInProgress);
	initializationInProgress = true;
}

void GraphicsPipelineStateBlobWriter::registerBytecodeBlob(ShaderType type, uint32 blobChecksum)
{
	XAssert(initializationInProgress);

	switch (type)
	{
		case ShaderType::Vertex:
			XAssert(!vsBytecodeRegistered);
			vsBytecodeRegistered = true;
			vsBytecodeChecksum = blobChecksum;
			break;

		case ShaderType::Amplification:
			XAssert(!asBytecodeRegistered);
			asBytecodeRegistered = true;
			asBytecodeChecksum = blobChecksum;
			break;

		case ShaderType::Mesh:
			XAssert(!msBytecodeRegistered);
			msBytecodeRegistered = true;
			msBytecodeChecksum = blobChecksum;
			break;

		case ShaderType::Pixel:
			XAssert(!psBytecodeRegistered);
			psBytecodeRegistered = true;
			psBytecodeChecksum = blobChecksum;
			break;

		default:
			XAssertUnreachableCode();
	}
}

void GraphicsPipelineStateBlobWriter::addRenderTarget(TexelViewFormat format)
{
	XAssert(initializationInProgress);
	XAssert(renderTargetCount < MaxRenderTargetCount);

	renderTargetFormats[renderTargetCount] = format;
	renderTargetCount++;
}

void GraphicsPipelineStateBlobWriter::setDepthStencilFormat(DepthStencilFormat format)
{
	XAssert(initializationInProgress);
	depthStencilFormat = format;
}

void GraphicsPipelineStateBlobWriter::enableVertexBuffer(uint8 index, bool perInstance)
{
	XAssert(initializationInProgress);
	static_assert(MaxVertexBufferCount <= 8); // uint8
	XAssert(index < MaxVertexBufferCount);
	const uint8 bit = 1 << index;
	vertexBuffersEnabledFlagBits |= bit;
	vertexBuffersPerInstanceFlagBits |= perInstance ? bit : 0;
}

void GraphicsPipelineStateBlobWriter::addVertexBinding(const char* name, uintptr nameLength, TexelViewFormat format, uint16 offset, uint8 bufferIndex)
{
	XAssert(initializationInProgress);
	XAssert(nameLength > 0 && nameLength <= MaxVertexBindingNameLength);
	XAssert(offset < MaxVertexBufferElementSize);
	XAssert(bufferIndex < MaxVertexBufferCount);

	const uint8 bufferBit = 1 << bufferIndex;
	XAssert((vertexBuffersEnabledFlagBits & bufferBit) != 0); // Buffer should be enabled previously
	vertexBuffersUsedFlagBits |= bufferBit;

	XAssert(vertexBindingCount < MaxVertexBindingCount);
	VertexBindingRecord& vertexBindingRecord = vertexBindingRecords[vertexBindingCount];
	vertexBindingCount++;

	static_assert(MaxVertexBufferElementSize <= (1 << 13));
	static_assert(MaxVertexBufferCount <= (1 << 3));
	XAssert((offset & 0b1110'0000'0000'0000) == 0);
	XAssert((bufferIndex & 0b0111) == 0);
	vertexBindingRecord.offset13_bufferIndex3 = offset | (uint16(bufferIndex) << 13);
	vertexBindingRecord.format = format;

	for (uintptr i = 0; i < countof(vertexBindingRecord.name); i++)
	{
		if (i < nameLength)
		{
			XAssert(name[i] > 0 && name[i] < 128);
			vertexBindingRecord.name[i] = name[i];
		}
		else
			vertexBindingRecord.name[i] = 0;
	}
}

void GraphicsPipelineStateBlobWriter::endInitialization()
{
	XAssert(initializationInProgress);
	initializationInProgress = false;

	// At least one bytecode should be registered.
	XAssert(vsBytecodeRegistered || asBytecodeRegistered || msBytecodeRegistered || psBytecodeRegistered);

	memoryBlockSize =
		sizeof(GenericBlobHeader) +
		sizeof(GraphicsPipelineBaseBlobBody) +
		sizeof(VertexBindingRecord) * vertexBindingCount;
}

uint32 GraphicsPipelineStateBlobWriter::getMemoryBlockSize() const
{
	XAssert(memoryBlockSize > 0);
	return memoryBlockSize;
}

void GraphicsPipelineStateBlobWriter::finalizeToMemoryBlock(void* memoryBlock, uint32 memoryBlockSize, uint32 pipelineLayoutSourceHash)
{
	XAssert(this->memoryBlockSize == memoryBlockSize && memoryBlockSize > 0);
	memorySet(memoryBlock, 0, memoryBlockSize);

	GraphicsPipelineBaseBlobBody& body = *(GraphicsPipelineBaseBlobBody*)((byte*)memoryBlock + sizeof(GenericBlobHeader));

	body.pipelineLayoutSourceHash = pipelineLayoutSourceHash;

	body.vsBytecodeChecksum = vsBytecodeChecksum;
	body.asBytecodeChecksum = asBytecodeChecksum;
	body.msBytecodeChecksum = msBytecodeChecksum;
	body.psBytecodeChecksum = psBytecodeChecksum;
	body.vsBytecodeRegistered = vsBytecodeRegistered;
	body.asBytecodeRegistered = asBytecodeRegistered;
	body.msBytecodeRegistered = msBytecodeRegistered;
	body.psBytecodeRegistered = psBytecodeRegistered;

	for (uint32 i = 0; i < MaxRenderTargetCount; i++)
		body.renderTargetFormats[i] = i < renderTargetCount ? renderTargetFormats[i] : TexelViewFormat::Undefined;

	body.depthStencilFormat = depthStencilFormat;

	body.vertexBuffersUsedFlagBits = vertexBuffersUsedFlagBits;
	body.vertexBuffersPerInstanceFlagBits = vertexBuffersPerInstanceFlagBits;
	body.vertexBindingCount = vertexBindingCount;

	const uint32 vertexBindingRecordsOffset =
		sizeof(GenericBlobHeader) +
		sizeof(GraphicsPipelineBaseBlobBody);
	VertexBindingRecord* dstVertexBindingRecords =
		(VertexBindingRecord*)((byte*)memoryBlock + vertexBindingRecordsOffset);
	memoryCopy(dstVertexBindingRecords, vertexBindingRecords, sizeof(VertexBindingRecord) * vertexBindingCount);

	FillGenericBlobHeader(memoryBlock, memoryBlockSize, GraphicsPipelineStateBlobSignature, 0);
}

// GraphicsPipelineStateBlobReader /////////////////////////////////////////////////////////////////

bool GraphicsPipelineStateBlobReader::open(const void* data, uint32 size)
{
	XAssert(!this->data);

	if (!ValidateGenericBlobHeader(data, size, GraphicsPipelineStateBlobSignature, 0))
		return false;
	if (size < sizeof(GenericBlobHeader) + sizeof(GraphicsPipelineBaseBlobBody))
		return false;

	const GraphicsPipelineBaseBlobBody* body =
		(const GraphicsPipelineBaseBlobBody*)((const byte*)data + sizeof(GenericBlobHeader));

	if (body->vertexBindingCount > MaxVertexBindingCount)
		return false;

	const uint32 vertexBindingRecordsOffset =
		sizeof(GenericBlobHeader) +
		sizeof(GraphicsPipelineBaseBlobBody);
	const uint32 sizeCheck =
		vertexBindingRecordsOffset +
		sizeof(VertexBindingRecord) * body->vertexBindingCount;

	if (size != sizeCheck)
		return false;

	this->data = data;
	this->size = size;
	this->body = body;
	this->vertexBindingRecords = body->vertexBindingCount > 0 ?
		(const VertexBindingRecord*)((const byte*)data + vertexBindingRecordsOffset) : nullptr;
	return true;
}

bool GraphicsPipelineStateBlobReader::isBytecodeBlobRegistered(ShaderType type) const
{
	switch (type)
	{
		case ShaderType::Vertex:		return !!body->vsBytecodeRegistered;
		case ShaderType::Amplification:	return !!body->asBytecodeRegistered;
		case ShaderType::Mesh:			return !!body->msBytecodeRegistered;
		case ShaderType::Pixel:			return !!body->psBytecodeRegistered;
		default:
			XAssertUnreachableCode();
	}
	return false;
}

uint32 GraphicsPipelineStateBlobReader::getBytecodeBlobChecksum(ShaderType type) const
{
	switch (type)
	{
		case ShaderType::Vertex:		XAssert(body->vsBytecodeRegistered); return body->vsBytecodeChecksum;
		case ShaderType::Amplification:	XAssert(body->asBytecodeRegistered); return body->asBytecodeChecksum;
		case ShaderType::Mesh:			XAssert(body->msBytecodeRegistered); return body->msBytecodeChecksum;
		case ShaderType::Pixel:			XAssert(body->psBytecodeRegistered); return body->psBytecodeChecksum;
		default:
			XAssertUnreachableCode();
	}
	return 0;
}

uint32 GraphicsPipelineStateBlobReader::getRenderTargetCount() const
{
	for (uint32 i = 0; i < MaxRenderTargetCount; i++)
	{
		if (body->renderTargetFormats[i] == TexelViewFormat::Undefined)
			return i;
	}
	return MaxRenderTargetCount;
}

VertexBindingInfo GraphicsPipelineStateBlobReader::getVertexBinding(uint8 bindingIndex) const
{
	XAssert(bindingIndex < body->vertexBindingCount);
	const VertexBindingRecord& record = vertexBindingRecords[bindingIndex];

	uintptr nameLength = 0;
	for (char c : record.name)
	{
		if (c > 0)
			nameLength++;
		else
			break;
	}

	VertexBindingInfo result = {};
	result.name = record.name;
	result.nameLength = nameLength;
	result.offset = record.offset13_bufferIndex3 & 0x1FFF;
	result.format = record.format;
	result.bufferIndex = record.offset13_bufferIndex3 >> 13;
	return result;
}

// BytecodeBlobWriter //////////////////////////////////////////////////////////////////////////////

void BytecodeBlobWriter::initialize(uint32 bytecodeSize)
{
	XAssert(memoryBlockSize == 0); // Not initialized.
	XAssert(bytecodeSize > 0);

	this->bytecodeSize = bytecodeSize;

	memoryBlockSize =
		sizeof(GenericBlobHeader) +
		sizeof(BytecodeBlobSubHeader) +
		bytecodeSize;
}

uint32 BytecodeBlobWriter::getMemoryBlockSize() const
{
	XAssert(memoryBlockSize > 0);
	return memoryBlockSize;
}

void BytecodeBlobWriter::setMemoryBlock(void* memoryBlock, uint32 memoryBlockSize)
{
	XAssert(!this->memoryBlock);
	XAssert(this->memoryBlockSize == memoryBlockSize && memoryBlockSize > 0);
	this->memoryBlock = memoryBlock;
	memorySet(memoryBlock, 0, memoryBlockSize);
}

void BytecodeBlobWriter::writeBytecode(const void* bytecodeData, uint32 bytecodeSize)
{
	XAssert(memoryBlock);
	XAssert(this->bytecodeSize == bytecodeSize);

	const uint32 offset =
		sizeof(GenericBlobHeader) +
		sizeof(BytecodeBlobSubHeader);
	memoryCopy((byte*)memoryBlock + offset, bytecodeData, bytecodeSize);
}

void BytecodeBlobWriter::finalize(ShaderType type, uint32 pipelineLayoutSourceHash)
{
	XAssert(memoryBlock);

	BytecodeBlobSubHeader& subHeader = *(BytecodeBlobSubHeader*)((byte*)memoryBlock + sizeof(GenericBlobHeader));
	subHeader.pipelineLayoutSourceHash = pipelineLayoutSourceHash;
	subHeader.type = type;

	FillGenericBlobHeader(memoryBlock, memoryBlockSize, BytecodeBlobSignature, 0);
	memoryBlock = nullptr;
}

// BytecodeBlobReader //////////////////////////////////////////////////////////////////////////////

bool BytecodeBlobReader::open(const void* data, uint32 size)
{
	XAssert(!this->data);

	if (!ValidateGenericBlobHeader(data, size, BytecodeBlobSignature, 0))
		return false;
	if (size <= sizeof(GenericBlobHeader) + sizeof(BytecodeBlobSubHeader))
		return false;

	this->data = data;
	this->size = size;
	this->subHeader = (const BytecodeBlobSubHeader*)((const byte*)data + sizeof(GenericBlobHeader));
	this->bytecodeData = subHeader + 1;
	this->bytecodeSize = size - (sizeof(GenericBlobHeader) + sizeof(BytecodeBlobSubHeader));
	return true;
}
