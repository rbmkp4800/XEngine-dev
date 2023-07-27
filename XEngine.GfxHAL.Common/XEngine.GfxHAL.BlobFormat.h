#pragma once

#include <XLib.h>

#include "XEngine.GfxHAL.Common.h"

// TODO: Revisit `GraphicsPipelineStateBlob`. For now temporary dirty implementaion.
// TODO: Probably we should remove all pseudo-error-reporting via `return false` from readers and just do master assert?

namespace XEngine::GfxHAL::BlobFormat::Data
{
	constexpr uint32 GenericBlobSignature = 0x874C2131;
	constexpr uint16 DescriptorSetLayoutBlobSignature	= 0xF1A6;
	constexpr uint16 PipelineLayoutBlobSignature		= 0xE1B2;
	constexpr uint16 GraphicsPipelineStateBlobSignature	= 0xBCF2;
	constexpr uint16 BytecodeBlobSignature				= 0xE084;

	struct GenericBlobHeader // 16 bytes
	{
		uint32 genericSignature;
		uint32 size;
		uint32 checksum; // CRC-32/zlib
		uint16 specificSignature;
		uint16 version;
	};
	static_assert(sizeof(GenericBlobHeader) == 16);

	struct DescriptorSetLayoutBlobSubHeader
	{
		uint32 sourceHash;
		uint16 bindingCount;
		uint16 _reserved;
	};

	struct DescriptorSetBindingRecord // 12 bytes
	{
		uint32 nameXSH0;
		uint32 nameXSH1;
		uint16 descriptorCount;
		DescriptorType descriptorType;
	};
	static_assert(sizeof(DescriptorSetBindingRecord) == 12);

	struct PipelineLayoutBlobSubHeader
	{
		uint32 sourceHash;
		uint16 bindingCount;
		uint16 platformDataSize;
	};

	struct PipelineBindingRecord // 16 bytes
	{
		// [0..8)
		uint64 nameXSH;
		// [8..12)
		uint32 descriptorSetLayoutSourceHash;
		// [12..14)
		uint16 d3dRootParameterIndex;
		// [14..15)
		PipelineBindingType type;
		// [15..16)
		uint8 inplaceConstantCount;
	};
	static_assert(sizeof(PipelineBindingRecord) == 16);

	struct GraphicsPipelineStateBlobBody // 36 bytes
	{
		// [0..4)
		uint32 pipelineLayoutSourceHash;

		// [4..24)
		uint32 vsBytecodeChecksum;
		uint32 asBytecodeChecksum;
		uint32 msBytecodeChecksum;
		uint32 psBytecodeChecksum;
		bool vsBytecodeRegistered;
		bool asBytecodeRegistered;
		bool msBytecodeRegistered;
		bool psBytecodeRegistered;

		// [24..33)
		TexelViewFormat renderTargetFormats[MaxRenderTargetCount];
		DepthStencilFormat depthStencilFormat;

		// [33..36)
		uint8 vertexBuffersUsedFlagBits;
		uint8 vertexBuffersPerInstanceFlagBits;
		uint8 vertexBindingCount;
	};
	static_assert(sizeof(GraphicsPipelineStateBlobBody) == 36);

	struct VertexBindingRecord // 24 bytes
	{
		char name[20];
		uint16 offset;
		TexelViewFormat format;
		uint8 bufferIndex;
	};
	static_assert(sizeof(VertexBindingRecord) == 24);

	struct BytecodeBlobSubHeader
	{
		uint32 pipelineLayoutSourceHash;
		ShaderType type;
	};
}

namespace XEngine::GfxHAL::BlobFormat
{
	struct DescriptorSetBindingInfo
	{
		uint64 nameXSH;
		uint16 descriptorCount;
		DescriptorType descriptorType;
	};

	struct PipelineBindingInfo
	{
		uint64 nameXSH;
		uint16 d3dRootParameterIndex;
		PipelineBindingType type;

		union
		{
			uint8 inplaceConstantCount;
			uint32 descriptorSetLayoutSourceHash;
		};
	};

	using VertexBindingInfo = Data::VertexBindingRecord;

	class GenericBlobReader
	{
	protected:
		const void* data = nullptr;
		uint32 size = 0;

	public:
		GenericBlobReader() = default;
		~GenericBlobReader() = default;

		uint32 getChecksum() const { return ((const Data::GenericBlobHeader*)data)->checksum; }
		bool validateChecksum() const;
	};

	class DescriptorSetLayoutBlobWriter
	{
	private:
		void* memoryBlock = nullptr;
		uint32 memoryBlockSize = 0;

		uint16 bindingCount = 0;

	public:
		DescriptorSetLayoutBlobWriter() = default;
		~DescriptorSetLayoutBlobWriter() = default;

		void initialize(uint16 bindingCount);

		uint32 getMemoryBlockSize() const;
		void setMemoryBlock(void* memoryBlock, uint32 memoryBlockSize);
		void writeBinding(uint16 bindingIndex, const DescriptorSetBindingInfo& bindingInfo);
		void finalize(uint32 sourceHash);
	};

	class DescriptorSetLayoutBlobReader : public GenericBlobReader
	{
	private:
		const Data::DescriptorSetLayoutBlobSubHeader* subHeader = nullptr;
		const Data::DescriptorSetBindingRecord* bindingRecords = nullptr;

	public:
		DescriptorSetLayoutBlobReader() = default;
		~DescriptorSetLayoutBlobReader() = default;

		bool open(const void* data, uint32 size);
		uint32 getSourceHash() const { return subHeader->sourceHash; }
		uint16 getBindingCount() const { return subHeader->bindingCount; }
		DescriptorSetBindingInfo getBinding(uint32 bindingIndex) const;
	};

	class PipelineLayoutBlobWriter
	{
	private:
		void* memoryBlock = nullptr;
		uint32 memoryBlockSize = 0;

		uint16 bindingCount = 0;
		uint32 platformDataSize = 0;

	public:
		PipelineLayoutBlobWriter() = default;
		~PipelineLayoutBlobWriter() = default;

		void initialize(uint16 bindingCount, uint32 platformDataSize);

		uint32 getMemoryBlockSize() const;
		void setMemoryBlock(void* memoryBlock, uint32 memoryBlockSize);
		void writeBinding(uint16 bindingIndex, const PipelineBindingInfo& bindingInfo);
		void writePlatformData(const void* platformData, uint32 platformDataSize);
		void finalize(uint32 sourceHash);
	};

	class PipelineLayoutBlobReader : public GenericBlobReader
	{
	private:
		const Data::PipelineLayoutBlobSubHeader* subHeader = nullptr;
		const Data::PipelineBindingRecord* bindingRecords = nullptr;
		const void* platformData = nullptr;

	public:
		PipelineLayoutBlobReader() = default;
		~PipelineLayoutBlobReader() = default;

		bool open(const void* data, uint32 size);

		uint32 getSourceHash() const { return subHeader->sourceHash; }
		uint16 getPipelineBindingCount() const { return subHeader->bindingCount; }
		PipelineBindingInfo getPipelineBinding(uint16 bindingIndex) const;
		uint32 getPlatformDataSize() const { return subHeader->platformDataSize; }
		const void* getPlatformData() const { return platformData; }
	};

	class GraphicsPipelineStateBlobWriter
	{
	private:
		uint32 vsBytecodeChecksum = 0;
		uint32 asBytecodeChecksum = 0;
		uint32 msBytecodeChecksum = 0;
		uint32 psBytecodeChecksum = 0;
		bool vsBytecodeRegistered = false;
		bool asBytecodeRegistered = false;
		bool msBytecodeRegistered = false;
		bool psBytecodeRegistered = false;

		DepthStencilFormat depthStencilFormat = DepthStencilFormat::Undefined;
		TexelViewFormat renderTargetFormats[MaxRenderTargetCount] = {};
		uint8 renderTargetCount = 0;

		Data::VertexBindingRecord vertexBindingRecords[MaxVertexBindingCount] = {};
		uint8 vertexBuffersEnabledFlagBits = 0;
		uint8 vertexBuffersUsedFlagBits = 0;
		uint8 vertexBuffersPerInstanceFlagBits = 0;
		uint8 vertexBindingCount = 0;

		bool initializationInProgress = false;

		uint32 memoryBlockSize = 0;

	public:
		GraphicsPipelineStateBlobWriter() = default;
		~GraphicsPipelineStateBlobWriter() = default;

		void beginInitialization();
		void registerBytecodeBlob(ShaderType type, uint32 blobChecksum);
		void addRenderTarget(TexelViewFormat format);
		void setDepthStencilFormat(DepthStencilFormat format);
		void enableVertexBuffer(uint8 index, bool perInstance);
		void addVertexBinding(const VertexBindingInfo& bindingInfo);
		void endInitialization();

		uint32 getMemoryBlockSize() const;
		void finalizeToMemoryBlock(void* memoryBlock, uint32 memoryBlockSize, uint32 pipelineLayoutSourceHash);
	};

	class GraphicsPipelineStateBlobReader : public GenericBlobReader
	{
	private:
		const Data::GraphicsPipelineStateBlobBody* body = nullptr;
		const Data::VertexBindingRecord* vertexBindingRecords = nullptr;

	public:
		GraphicsPipelineStateBlobReader() = default;
		~GraphicsPipelineStateBlobReader() = default;

		bool open(const void* data, uint32 size);

		bool isVSBytecodeBlobRegistered() const { return body->vsBytecodeRegistered; }
		bool isASBytecodeBlobRegistered() const { return body->asBytecodeRegistered; }
		bool isMSBytecodeBlobRegistered() const { return body->msBytecodeRegistered; }
		bool isPSBytecodeBlobRegistered() const { return body->psBytecodeRegistered; }
		uint32 getVSBytecodeBlobChecksum() const { return body->vsBytecodeChecksum; }
		uint32 getASBytecodeBlobChecksum() const { return body->asBytecodeChecksum; }
		uint32 getMSBytecodeBlobChecksum() const { return body->msBytecodeChecksum; }
		uint32 getPSBytecodeBlobChecksum() const { return body->psBytecodeChecksum; }

		uint32 getPipelineLayoutSourceHash() const { return body->pipelineLayoutSourceHash; }
		uint32 getRenderTargetCount() const;
		TexelViewFormat getRenderTargetFormat(uint32 index) const { XAssert(index < MaxRenderTargetCount); return body->renderTargetFormats[index]; }
		DepthStencilFormat getDepthStencilFormat() const { return body->depthStencilFormat; }
		uint8 getVertexBindingCount() const { return body->vertexBindingCount; }
		VertexBindingInfo getVertexBinding(uint8 bindingIndex) const;
	};

	class BytecodeBlobWriter
	{
	private:
		void* memoryBlock = nullptr;
		uint32 memoryBlockSize = 0;

		uint32 bytecodeSize = 0;

	public:
		BytecodeBlobWriter() = default;
		~BytecodeBlobWriter() = default;

		void initialize(uint32 bytecodeSize);

		uint32 getMemoryBlockSize() const;
		void setMemoryBlock(void* memoryBlock, uint32 memoryBlockSize);
		void writeBytecode(const void* bytecodeData, uint32 bytecodeSize);
		void finalize(ShaderType type, uint32 pipelineLayoutSourceHash);
	};

	class BytecodeBlobReader : public GenericBlobReader
	{
	private:
		const Data::BytecodeBlobSubHeader* subHeader = nullptr;
		const void* bytecodeData = nullptr;
		uint32 bytecodeSize = 0;

	public:
		BytecodeBlobReader() = default;
		~BytecodeBlobReader() = default;

		bool open(const void* data, uint32 size);

		ShaderType getType() const { return subHeader->type; }
		uint32 getPipelineLayoutSourceHash() const { return subHeader->pipelineLayoutSourceHash; }
		uint32 getBytecodeSize() const { return bytecodeSize; }
		const void* getBytecodeData() const { return bytecodeData; }
	};
}
