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
	XAssert(bindingCount <= MaxDescriptorSetNestedBindingCount);

	this->bindingCount = bindingCount;

	memoryBlockSize =
		sizeof(GenericBlobHeader) +
		sizeof(DescriptorSetLayoutBlobSubHeader) +
		sizeof(DescriptorSetNestedBindingRecord) * bindingCount;
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

void DescriptorSetLayoutBlobWriter::writeBinding(uint16 bindingIndex, const DescriptorSetNestedBindingInfo& bindingInfo)
{
	XAssert(memoryBlock);
	XAssert(bindingIndex < bindingCount);

	const uint32 offset =
		sizeof(GenericBlobHeader) +
		sizeof(DescriptorSetLayoutBlobSubHeader) +
		sizeof(DescriptorSetNestedBindingRecord) * bindingIndex;
	DescriptorSetNestedBindingRecord& bindingRecord = *(DescriptorSetNestedBindingRecord*)((byte*)memoryBlock + offset);
	bindingRecord.nameXSH0 = uint32(bindingInfo.nameXSH);
	bindingRecord.nameXSH1 = uint32(bindingInfo.nameXSH >> 32);
	bindingRecord.descriptorCount = bindingInfo.descriptorCount;
	bindingRecord.descriptorType = bindingInfo.descriptorType;
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
		subHeader->bindingCount > MaxDescriptorSetNestedBindingCount)
		return false;

	const uint32 bindingRecordsOffset =
		sizeof(GenericBlobHeader) +
		sizeof(DescriptorSetLayoutBlobSubHeader);
	const uint32 sizeCheck =
		bindingRecordsOffset +
		sizeof(DescriptorSetNestedBindingRecord) * subHeader->bindingCount;

	if (size != sizeCheck)
		return false;

	this->data = data;
	this->size = size;
	this->subHeader = subHeader;
	this->bindingRecords = (const DescriptorSetNestedBindingRecord*)((const byte*)data + bindingRecordsOffset);
	return true;
}

DescriptorSetNestedBindingInfo DescriptorSetLayoutBlobReader::getBinding(uint32 bindingIndex) const
{
	XAssert(bindingIndex < subHeader->bindingCount);
	const DescriptorSetNestedBindingRecord& bindingRecord = bindingRecords[bindingIndex];

	DescriptorSetNestedBindingInfo result = {};
	result.nameXSH = uint64(bindingRecord.nameXSH0) | (uint64(bindingRecord.nameXSH1) << 32);
	result.descriptorCount = bindingRecord.descriptorCount;
	result.descriptorType = bindingRecord.descriptorType;
	return result;
}

// PipelineLayoutBlobWriter ////////////////////////////////////////////////////////////////////////

void PipelineLayoutBlobWriter::beginInitialization()
{
	XAssert(memoryBlockSize == 0);	// Not initialized.
	XAssert(!initializationInProgress);
	initializationInProgress = true;
}

void PipelineLayoutBlobWriter::addPipelineBinding(const PipelineBindingInfo& bindingInfo)
{
	XAssert(initializationInProgress);

	XAssert(pipelineBindingCount < MaxPipelineBindingCount);
	PipelineBindingRecord& bindingRecord = pipelineBindingRecords[pipelineBindingCount];
	pipelineBindingCount++;

	bindingRecord.nameXSH = bindingInfo.nameXSH;
	bindingRecord.platformBindingIndex = bindingInfo.platformBindingIndex;
	bindingRecord.type = bindingInfo.type;
	if (bindingInfo.type == PipelineBindingType::DescriptorSet)
		bindingRecord.descriptorSetLayoutSourceHash = bindingInfo.descriptorSetLayoutSourceHash;
	if (bindingInfo.type == PipelineBindingType::Constants)
		bindingRecord.constantsSize = bindingInfo.constantsSize;
}

void PipelineLayoutBlobWriter::endInitialization(uint32 platformDataSize)
{
	XAssert(initializationInProgress);
	initializationInProgress = false;

	this->platformDataSize = platformDataSize;

	memoryBlockSize =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineLayoutBlobSubHeader) +
		sizeof(PipelineBindingRecord) * pipelineBindingCount +
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

void PipelineLayoutBlobWriter::writePlatformData(const void* platformData, uint32 platformDataSize)
{
	XAssert(memoryBlock);
	XAssert(this->platformDataSize == platformDataSize);

	const uint32 offset =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineLayoutBlobSubHeader) +
		sizeof(PipelineBindingRecord) * pipelineBindingCount;
	memoryCopy((byte*)memoryBlock + offset, platformData, platformDataSize);
}

void PipelineLayoutBlobWriter::finalize(uint32 sourceHash)
{
	XAssert(memoryBlock);

	PipelineLayoutBlobSubHeader& subHeader =
		*(PipelineLayoutBlobSubHeader*)((byte*)memoryBlock + sizeof(GenericBlobHeader));
	subHeader.sourceHash = sourceHash;
	subHeader.bindingCount = uint16(pipelineBindingCount);
	subHeader.platformDataSize = uint16(platformDataSize);
	XAssert(subHeader.platformDataSize == platformDataSize);

	const uint32 pipelineBindingRecordsOffset =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineLayoutBlobSubHeader);

	PipelineBindingRecord* dstPipelineBindingRecords =
		(PipelineBindingRecord*)((byte*)memoryBlock + pipelineBindingRecordsOffset);
	
	memoryCopy(dstPipelineBindingRecords, pipelineBindingRecords, sizeof(PipelineBindingRecord) * pipelineBindingCount);

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
	const PipelineBindingRecord& bindingRecord = bindingRecords[bindingIndex];

	PipelineBindingInfo result = {};
	result.nameXSH = bindingRecord.nameXSH;
	result.platformBindingIndex = bindingRecord.platformBindingIndex;
	result.type = bindingRecord.type;
	if (bindingRecord.type == PipelineBindingType::Constants)
		result.constantsSize = bindingRecord.constantsSize;
	if (bindingRecord.type == PipelineBindingType::DescriptorSet)
		result.descriptorSetLayoutSourceHash = bindingRecord.descriptorSetLayoutSourceHash;
	return result;
}

// GraphicsPipelineBaseBlobWriter //////////////////////////////////////////////////////////////////

void GraphicsPipelineBaseBlobWriter::beginInitialization()
{
	XAssert(memoryBlockSize == 0);	// Not initialized.
	XAssert(!initializationInProgress);
	initializationInProgress = true;
}

void GraphicsPipelineBaseBlobWriter::registerBytecodeBlob(BytecodeBlobType type, uint32 blobChecksum)
{
	XAssert(initializationInProgress);

	switch (type)
	{
		case BytecodeBlobType::VertexShader:
			XAssert(!vsBytecodeRegistered);
			vsBytecodeRegistered = true;
			vsBytecodeChecksum = blobChecksum;
			break;

		case BytecodeBlobType::AmplificationShader:
			XAssert(!asBytecodeRegistered);
			asBytecodeRegistered = true;
			asBytecodeChecksum = blobChecksum;
			break;

		case BytecodeBlobType::MeshShader:
			XAssert(!msBytecodeRegistered);
			msBytecodeRegistered = true;
			msBytecodeChecksum = blobChecksum;
			break;

		case BytecodeBlobType::PixelShader:
			XAssert(!psBytecodeRegistered);
			psBytecodeRegistered = true;
			psBytecodeChecksum = blobChecksum;
			break;

		default:
			XAssertUnreachableCode();
	}
}

void GraphicsPipelineBaseBlobWriter::addRenderTarget(TexelViewFormat format)
{
	XAssert(initializationInProgress);
	XAssert(renderTargetCount < MaxRenderTargetCount);

	renderTargetFormats[renderTargetCount] = format;
	renderTargetCount++;
}

void GraphicsPipelineBaseBlobWriter::setDepthStencilFormat(DepthStencilFormat format)
{
	XAssert(initializationInProgress);
	depthStencilFormat = format;
}

void GraphicsPipelineBaseBlobWriter::enableVertexBuffer(uint8 index, bool perInstance)
{
	XAssert(initializationInProgress);
	static_assert(MaxVertexBufferCount <= 8); // uint8
	XAssert(index < MaxVertexBufferCount);
	const uint8 bit = 1 << index;
	vertexBuffersEnabledFlagBits |= bit;
	vertexBuffersPerInstanceFlagBits |= perInstance ? bit : 0;
}

void GraphicsPipelineBaseBlobWriter::addVertexBinding(const char* name, uintptr nameLength, TexelViewFormat format, uint16 offset, uint8 bufferIndex)
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

void GraphicsPipelineBaseBlobWriter::endInitialization()
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

uint32 GraphicsPipelineBaseBlobWriter::getMemoryBlockSize() const
{
	XAssert(memoryBlockSize > 0);
	return memoryBlockSize;
}

void GraphicsPipelineBaseBlobWriter::finalizeToMemoryBlock(void* memoryBlock, uint32 memoryBlockSize, uint32 pipelineLayoutSourceHash)
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

	FillGenericBlobHeader(memoryBlock, memoryBlockSize, GraphicsPipelineBaseBlobSignature, 0);
}

// GraphicsPipelineBaseBlobReader //////////////////////////////////////////////////////////////////

bool GraphicsPipelineBaseBlobReader::open(const void* data, uint32 size)
{
	XAssert(!this->data);

	if (!ValidateGenericBlobHeader(data, size, GraphicsPipelineBaseBlobSignature, 0))
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

bool GraphicsPipelineBaseBlobReader::isBytecodeBlobRegistered(BytecodeBlobType type) const
{
	switch (type)
	{
		case BytecodeBlobType::VertexShader:		return !!body->vsBytecodeRegistered;
		case BytecodeBlobType::AmplificationShader:	return !!body->asBytecodeRegistered;
		case BytecodeBlobType::MeshShader:			return !!body->msBytecodeRegistered;
		case BytecodeBlobType::PixelShader:			return !!body->psBytecodeRegistered;
		default:
			XAssertUnreachableCode();
	}
	return false;
}

uint32 GraphicsPipelineBaseBlobReader::getBytecodeBlobChecksum(BytecodeBlobType type) const
{
	switch (type)
	{
		case BytecodeBlobType::VertexShader:		XAssert(body->vsBytecodeRegistered); return body->vsBytecodeChecksum;
		case BytecodeBlobType::AmplificationShader:	XAssert(body->asBytecodeRegistered); return body->asBytecodeChecksum;
		case BytecodeBlobType::MeshShader:			XAssert(body->msBytecodeRegistered); return body->msBytecodeChecksum;
		case BytecodeBlobType::PixelShader:			XAssert(body->psBytecodeRegistered); return body->psBytecodeChecksum;
		default:
			XAssertUnreachableCode();
	}
	return 0;
}

uint32 GraphicsPipelineBaseBlobReader::getRenderTargetCount() const
{
	for (uint32 i = 0; i < MaxRenderTargetCount; i++)
	{
		if (body->renderTargetFormats[i] == TexelViewFormat::Undefined)
			return i;
	}
	return MaxRenderTargetCount;
}

VertexBindingInfo GraphicsPipelineBaseBlobReader::getVertexBinding(uint8 bindingIndex) const
{
	XAssert(bindingIndex < body->vertexBindingCount);
	const VertexBindingRecord& bindingRecord = vertexBindingRecords[bindingIndex];

	uintptr nameLength = 0;
	for (char c : bindingRecord.name)
	{
		if (c > 0)
			nameLength++;
		else
			break;
	}

	VertexBindingInfo result = {};
	result.name = bindingRecord.name;
	result.nameLength = nameLength;
	result.offset = bindingRecord.offset13_bufferIndex3 & 0x1FFF;
	result.format = bindingRecord.format;
	result.bufferIndex = bindingRecord.offset13_bufferIndex3 >> 13;
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

void BytecodeBlobWriter::finalize(BytecodeBlobType type, uint32 pipelineLayoutSourceHash)
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

// PipelineLayoutMetadataBlobWriter ////////////////////////////////////////////////////////////////

void PipelineLayoutMetadataBlobWriter::beginInitialization()
{
	XAssert(memoryBlockSize == 0);	// Not initialized.
	XAssert(!initializationInProgress);
	initializationInProgress = true;
}

void PipelineLayoutMetadataBlobWriter::addGenericPipelineBinding(uint16 shaderRegister)
{
	XAssert(initializationInProgress);
	XAssert(!nestededBindingsAdditionInProgress);

	XAssert(pipelineBindingCount < MaxPipelineBindingCount);
	PipelineBindingMetaRecord& pipelineBindingRecord = pipelineBindingRecords[pipelineBindingCount];
	pipelineBindingCount++;

	pipelineBindingRecord.shaderRegisterOrNestedBindingsOffset = shaderRegister;
	pipelineBindingRecord.nestedBindingCount = 0;
}

void PipelineLayoutMetadataBlobWriter::addDescriptorSetPipelineBindingAndBeginAddingNestedBindings()
{
	XAssert(initializationInProgress);
	XAssert(!nestededBindingsAdditionInProgress);

	XAssert(pipelineBindingCount < MaxPipelineBindingCount);
	PipelineBindingMetaRecord& pipelineBindingRecord = pipelineBindingRecords[pipelineBindingCount];
	pipelineBindingCount++;

	pipelineBindingRecord.shaderRegisterOrNestedBindingsOffset = nestedBindingCount;
	pipelineBindingRecord.nestedBindingCount = 0;

	nestededBindingsAdditionInProgress = true;
}

void PipelineLayoutMetadataBlobWriter::addDescriptorSetNestedBinding(
	const DescriptorSetNestedBindingInfo& nestedBindingInfo, uint16 shaderRegister)
{
	XAssert(initializationInProgress);
	XAssert(nestededBindingsAdditionInProgress);

	XAssert(pipelineBindingCount > 0);
	pipelineBindingRecords[pipelineBindingCount - 1].nestedBindingCount++;

	XAssert(nestedBindingCount < MaxTotalNestedBindingCount);
	DescriptorSetNestedBindingMetaRecord& nestedBinding = nestedBindingRecords[nestedBindingCount];
	nestedBindingCount++;

	nestedBinding.nameXSH = nestedBindingInfo.nameXSH;
	nestedBinding.descriptorCount = nestedBindingInfo.descriptorCount;
	nestedBinding.descriptorType = nestedBindingInfo.descriptorType;
	nestedBinding.shaderRegister = shaderRegister;
}

void PipelineLayoutMetadataBlobWriter::endAddingDescriptorSetNestedBindings()
{
	XAssert(initializationInProgress);
	XAssert(nestededBindingsAdditionInProgress);
	nestededBindingsAdditionInProgress = false;
	
	XAssert(pipelineBindingCount > 0);
	XAssert(pipelineBindingRecords[pipelineBindingCount - 1].nestedBindingCount > 0);
}

void PipelineLayoutMetadataBlobWriter::endInitialization()
{
	XAssert(initializationInProgress);
	XAssert(!nestededBindingsAdditionInProgress);
	initializationInProgress = false;

	memoryBlockSize =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineLayoutMetadataBlobSubHeader) +
		sizeof(PipelineBindingMetaRecord) * pipelineBindingCount +
		sizeof(DescriptorSetNestedBindingMetaRecord) * nestedBindingCount;
}

uint32 PipelineLayoutMetadataBlobWriter::getMemoryBlockSize() const
{
	XAssert(memoryBlockSize > 0);
	return memoryBlockSize;
}

void PipelineLayoutMetadataBlobWriter::finalizeToMemoryBlock(void* memoryBlock, uint32 memoryBlockSize, uint32 sourceHash)
{
	XAssert(this->memoryBlockSize == memoryBlockSize && memoryBlockSize > 0);
	memorySet(memoryBlock, 0, memoryBlockSize);

	PipelineLayoutMetadataBlobSubHeader& subHeader =
		*(PipelineLayoutMetadataBlobSubHeader*)((byte*)memoryBlock + sizeof(GenericBlobHeader));
	subHeader.sourceHash = sourceHash;
	subHeader.pipelineBindingCount = pipelineBindingCount;
	subHeader.nestedBindingCount = nestedBindingCount;

	const uint32 pipelineBindingRecordsOffset =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineLayoutBlobSubHeader);
	const uint32 nestedBindingRecordsOffset =
		pipelineBindingRecordsOffset +
		sizeof(PipelineBindingMetaRecord) * pipelineBindingCount;
	const uint32 sizeCheck =
		nestedBindingRecordsOffset +
		sizeof(DescriptorSetNestedBindingMetaRecord) * nestedBindingCount;

	XAssert(memoryBlockSize == sizeCheck);

	PipelineBindingMetaRecord* dstPipelineBindingRecords =
		(PipelineBindingMetaRecord*)((byte*)memoryBlock + pipelineBindingRecordsOffset);
	DescriptorSetNestedBindingMetaRecord* dstNestedBindingRecords =
		(DescriptorSetNestedBindingMetaRecord*)((byte*)memoryBlock + nestedBindingRecordsOffset);

	memoryCopy(dstPipelineBindingRecords, pipelineBindingRecords, sizeof(PipelineBindingMetaRecord) * pipelineBindingCount);
	memoryCopy(dstNestedBindingRecords, nestedBindingRecords, sizeof(DescriptorSetNestedBindingMetaRecord) * nestedBindingCount);

	FillGenericBlobHeader(memoryBlock, memoryBlockSize, PipelineLayoutMetadataBlobSignature, 0);
}

// PipelineLayoutMetadataBlobReader ////////////////////////////////////////////////////////////////

bool PipelineLayoutMetadataBlobReader::open(const void* data, uint32 size)
{
	XAssert(!this->data);

	if (!ValidateGenericBlobHeader(data, size, PipelineLayoutMetadataBlobSignature, 0))
		return false;
	if (size < sizeof(GenericBlobHeader) + sizeof(PipelineLayoutMetadataBlobSubHeader))
		return false;

	const PipelineLayoutMetadataBlobSubHeader* subHeader =
		(const PipelineLayoutMetadataBlobSubHeader*)((const byte*)data + sizeof(GenericBlobHeader));

	if (subHeader->pipelineBindingCount > MaxPipelineBindingCount)
		return false;

	const uint32 pipelineBindingRecordsOffset =
		sizeof(GenericBlobHeader) +
		sizeof(PipelineLayoutMetadataBlobSubHeader);
	const uint32 nestedBindingRecordsOffset =
		pipelineBindingRecordsOffset +
		sizeof(PipelineBindingMetaRecord) * subHeader->pipelineBindingCount;
	const uint32 sizeCheck =
		nestedBindingRecordsOffset +
		sizeof(DescriptorSetNestedBindingMetaRecord) * subHeader->nestedBindingCount;

	if (size != sizeCheck)
		return false;

	const PipelineBindingMetaRecord* pipelineBindingRecords =
		(const PipelineBindingMetaRecord*)((const byte*)data + pipelineBindingRecordsOffset);

	const DescriptorSetNestedBindingMetaRecord* nestedBindingRecords =
		(const DescriptorSetNestedBindingMetaRecord*)((const byte*)data + nestedBindingRecordsOffset);

	uint32 nestedBindingsCounter = 0;
	for (uint32 i = 0; i < subHeader->pipelineBindingCount; i++)
	{
		const PipelineBindingMetaRecord& pipelineBinding = pipelineBindingRecords[i];
		const bool hasNestedBindings = pipelineBinding.nestedBindingCount > 0;
		if (hasNestedBindings)
		{
			const uint32 nestedBindingsOffset = pipelineBinding.shaderRegisterOrNestedBindingsOffset;
			if (nestedBindingsCounter != nestedBindingsOffset)
				return false;
			nestedBindingsCounter += pipelineBinding.nestedBindingCount;
		}
	}

	if (nestedBindingsCounter != subHeader->nestedBindingCount)
		return false;

	this->data = data;
	this->size = size;
	this->subHeader = subHeader;
	this->pipelineBindingRecords = subHeader->pipelineBindingCount > 0 ? pipelineBindingRecords : nullptr;
	this->nestedBindingRecords = subHeader->nestedBindingCount > 0 ? nestedBindingRecords : nullptr;
	return true;
}

bool PipelineLayoutMetadataBlobReader::isDescriptorSetBinding(uint16 pipelingBindingIndex) const
{
	XAssert(pipelingBindingIndex < subHeader->pipelineBindingCount);
	return pipelineBindingRecords[pipelingBindingIndex].nestedBindingCount != 0;
}

uint16 PipelineLayoutMetadataBlobReader::getPipelineBindingShaderRegister(uint16 pipelingBindingIndex) const
{
	XAssert(pipelingBindingIndex < subHeader->pipelineBindingCount);
	XAssert(pipelineBindingRecords[pipelingBindingIndex].nestedBindingCount == 0);
	return pipelineBindingRecords[pipelingBindingIndex].shaderRegisterOrNestedBindingsOffset;
}

uint16 PipelineLayoutMetadataBlobReader::getDescriptorSetNestedBindingCount(uint16 pipelingBindingIndex) const
{
	XAssert(pipelingBindingIndex < subHeader->pipelineBindingCount);
	XAssert(pipelineBindingRecords[pipelingBindingIndex].nestedBindingCount != 0);
	return pipelineBindingRecords[pipelingBindingIndex].nestedBindingCount;
}

DescriptorSetNestedBindingMetaInfo PipelineLayoutMetadataBlobReader::getDescriptorSetNestedBinding(
	uint16 pipelingBindingIndex, uint16 descriptorSetNestedBindingIndex) const
{
	XAssert(pipelingBindingIndex < subHeader->pipelineBindingCount);
	XAssert(descriptorSetNestedBindingIndex < pipelineBindingRecords[pipelingBindingIndex].nestedBindingCount);
	const uint16 nestedBindingsOffset = pipelineBindingRecords[pipelingBindingIndex].shaderRegisterOrNestedBindingsOffset;
	const uint16 nestedBindingGlobalIndex = nestedBindingsOffset + descriptorSetNestedBindingIndex;
	XAssert(nestedBindingGlobalIndex < subHeader->nestedBindingCount);

	const DescriptorSetNestedBindingMetaRecord& nestedBindingRecord = nestedBindingRecords[nestedBindingGlobalIndex];

	DescriptorSetNestedBindingMetaInfo result = {};
	result.generic.nameXSH = nestedBindingRecord.nameXSH;
	result.generic.descriptorCount = nestedBindingRecord.descriptorCount;
	result.generic.descriptorType = nestedBindingRecord.descriptorType;
	result.shaderRegister = nestedBindingRecord.shaderRegister;
	return result;
}
