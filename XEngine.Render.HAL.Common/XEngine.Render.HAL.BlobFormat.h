#pragma once

#include <XLib.h>

#include "XEngine.Render.HAL.Common.h"

// TODO: Revist `GraphicsPipelineBaseBlob`. For now temporary dirty implementaion.

namespace XEngine::Render::HAL::BlobFormat
{
	enum class BytecodeBlobType : uint8
	{
		Undefined = 0,
		ComputeShader,
		VertexShader,
		AmplificationShader,
		MeshShader,
		PixelShader,
	};
}

namespace XEngine::Render::HAL::BlobFormat::Data
{
	constexpr uint32 GenericBlobSignature = 0x874C2131;
	constexpr uint16 DescriptorSetLayoutBlobSignature		= 0xF1A6;
	constexpr uint16 PipelineLayoutBlobSignature			= 0xE1B2;
	constexpr uint16 GraphicsPipelineBaseBlobSignature		= 0xBCF2;
	constexpr uint16 BytecodeBlobSignature					= 0xE084;
	constexpr uint16 PipelineLayoutMetadataBlobSignature	= 0x17B9;

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

	struct DescriptorSetNestedBindingRecord // 12 bytes
	{
		uint32 nameXSH0;
		uint32 nameXSH1;
		uint16 descriptorCount;
		DescriptorType descriptorType;
	};
	static_assert(sizeof(DescriptorSetNestedBindingRecord) == 12);

	struct PipelineLayoutBlobSubHeader
	{
		uint32 sourceHash;
		uint16 bindingCount;
		uint16 platformDataSize;
	};

	struct PipelineBindingRecord // 16 bytes
	{
		// 0..7
		uint64 nameXSH;
		// 8..11
		uint32 descriptorSetLayoutSourceHash;
		// 12..13
		uint16 platformBindingIndex;
		// 14
		PipelineBindingType type;
		// 15
		uint8 constantsSize;
	};
	static_assert(sizeof(PipelineBindingRecord) == 16);

	struct GraphicsPipelineBaseBlobBody
	{
		uint32 pipelineLayoutSourceHash;

		uint32 vsBytecodeChecksum;
		uint32 asBytecodeChecksum;
		uint32 msBytecodeChecksum;
		uint32 psBytecodeChecksum;
		bool vsBytecodeRegistered;
		bool asBytecodeRegistered;
		bool msBytecodeRegistered;
		bool psBytecodeRegistered;

		TexelViewFormat renderTargetFormats[MaxRenderTargetCount];
		DepthStencilFormat depthStencilFormat;
	};

	struct BytecodeBlobSubHeader
	{
		uint32 pipelineLayoutSourceHash;
		BytecodeBlobType type;
	};

	struct PipelineLayoutMetadataBlobSubHeader
	{
		uint32 sourceHash;
		uint16 pipelineBindingCount;
		uint16 nestedBindingCount;
	};

	struct PipelineBindingMetaRecord // 4 bytes
	{
		uint16 shaderRegisterOrNestedBindingsOffset;
		uint16 nestedBindingCount;
	};
	static_assert(sizeof(PipelineBindingMetaRecord) == 4);

	struct DescriptorSetNestedBindingMetaRecord // 16 bytes
	{
		uint64 nameXSH;
		uint16 descriptorCount;
		DescriptorType descriptorType;
		uint16 shaderRegister;
	};
	static_assert(sizeof(DescriptorSetNestedBindingMetaRecord) == 16);
}

namespace XEngine::Render::HAL::BlobFormat
{
	struct DescriptorSetNestedBindingInfo
	{
		uint64 nameXSH;
		uint16 descriptorCount;
		DescriptorType descriptorType;
	};

	struct PipelineBindingInfo
	{
		uint64 nameXSH;
		uint16 platformBindingIndex;
		PipelineBindingType type;

		union
		{
			uint8 constantsSize;
			uint32 descriptorSetLayoutSourceHash;
		};
	};

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
		void writeBinding(uint16 bindingIndex, const DescriptorSetNestedBindingInfo& bindingInfo);
		void finalize(uint32 sourceHash);
	};

	class DescriptorSetLayoutBlobReader : public GenericBlobReader
	{
	private:
		const Data::DescriptorSetLayoutBlobSubHeader* subHeader = nullptr;
		const Data::DescriptorSetNestedBindingRecord* bindingRecords = nullptr;

	public:
		DescriptorSetLayoutBlobReader() = default;
		~DescriptorSetLayoutBlobReader() = default;

		bool open(const void* data, uint32 size);
		uint32 getSourceHash() const { return subHeader->sourceHash; }
		uint16 getBindingCount() const { return subHeader->bindingCount; }
		DescriptorSetNestedBindingInfo getBinding(uint32 bindingIndex) const;
	};

	class PipelineLayoutBlobWriter
	{
	private:
		void* memoryBlock = nullptr;
		uint32 memoryBlockSize = 0;

		Data::PipelineBindingRecord pipelineBindingRecords[MaxPipelineBindingCount] = {};
		uint16 pipelineBindingCount = 0;
		uint32 platformDataSize = 0;
		bool initializationInProgress = false;

	public:
		PipelineLayoutBlobWriter() = default;
		~PipelineLayoutBlobWriter() = default;

		void beginInitialization();
		void addPipelineBinding(const PipelineBindingInfo& bindingInfo);
		void endInitialization(uint32 platformDataSize);

		uint32 getMemoryBlockSize() const;
		void setMemoryBlock(void* memoryBlock, uint32 memoryBlockSize);
		void writePlatformData(const void* platformData, uint32 platformDataSize);
		void finalize(uint32 sourceHash);

		inline uint16 getPipelineBingingCount() const { return pipelineBindingCount; }
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

	class GraphicsPipelineBaseBlobWriter
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

		bool initializationInProgress = false;

		uint32 memoryBlockSize = 0;

	public:
		GraphicsPipelineBaseBlobWriter() = default;
		~GraphicsPipelineBaseBlobWriter() = default;

		void beginInitialization();
		void registerBytecodeBlob(BytecodeBlobType type, uint32 blobChecksum);
		void addRenderTarget(TexelViewFormat format);
		void setDepthStencilFormat(DepthStencilFormat format);
		void endInitialization();

		uint32 getMemoryBlockSize() const;
		void finalizeToMemoryBlock(void* memoryBlock, uint32 memoryBlockSize, uint32 pipelineLayoutSourceHash);
	};

	class GraphicsPipelineBaseBlobReader : public GenericBlobReader
	{
	private:
		const Data::GraphicsPipelineBaseBlobBody* body = nullptr;

	public:
		GraphicsPipelineBaseBlobReader() = default;
		~GraphicsPipelineBaseBlobReader() = default;

		bool open(const void* data, uint32 size);

		uint32 getPipelineLayoutSourceHash() const { return body->pipelineLayoutSourceHash; }
		bool isBytecodeBlobRegistered(BytecodeBlobType type) const;
		uint32 getBytecodeBlobChecksum(BytecodeBlobType type) const;
		uint32 getRenderTargetCount() const;
		TexelViewFormat getRenderTargetFormat(uint32 index) const { XAssert(index < MaxRenderTargetCount); return body->renderTargetFormats[index]; }
		DepthStencilFormat getDepthStencilFormat() const { return body->depthStencilFormat; }
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
		void finalize(BytecodeBlobType type, uint32 pipelineLayoutSourceHash);
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

		BytecodeBlobType getType() const { return subHeader->type; }
		uint32 getPipelineLayoutSourceHash() const { return subHeader->pipelineLayoutSourceHash; }
		uint32 getBytecodeSize() const { return bytecodeSize; }
		const void* getBytecodeData() const { return bytecodeData; }
	};

	// Metadata blobs //////////////////////////////////////////////////////////////////////////////
	// These are used only in compiler.

	struct DescriptorSetNestedBindingMetaInfo
	{
		DescriptorSetNestedBindingInfo generic;
		uint16 shaderRegister;
	};

	class PipelineLayoutMetadataBlobWriter
	{
	private:
		static constexpr uint32 MaxTotalNestedBindingCount = 256;

	private:
		Data::PipelineBindingMetaRecord pipelineBindingRecords[MaxPipelineBindingCount] = {};
		Data::DescriptorSetNestedBindingMetaRecord nestedBindingRecords[MaxTotalNestedBindingCount] = {};
		uint16 pipelineBindingCount = 0;
		uint16 nestedBindingCount = 0;
		bool nestededBindingsAdditionInProgress = false;
		bool initializationInProgress = false;

		uint32 memoryBlockSize = 0;

	public:
		PipelineLayoutMetadataBlobWriter() = default;
		~PipelineLayoutMetadataBlobWriter() = default;

		void beginInitialization();
		void addGenericPipelineBinding(uint16 shaderRegister);
		void addDescriptorSetPipelineBindingAndBeginAddingNestedBindings();
		void addDescriptorSetNestedBinding(const DescriptorSetNestedBindingInfo& nestedBindingInfo, uint16 shaderRegister);
		void endAddingDescriptorSetNestedBindings();
		void endInitialization();

		uint32 getMemoryBlockSize() const;
		void finalizeToMemoryBlock(void* memoryBlock, uint32 memoryBlockSize, uint32 sourceHash);

		uint32 getPipelineBingingCount() const { return pipelineBindingCount; }
	};

	class PipelineLayoutMetadataBlobReader : public GenericBlobReader
	{
	private:
		const Data::PipelineLayoutMetadataBlobSubHeader* subHeader = nullptr;
		const Data::PipelineBindingMetaRecord* pipelineBindingRecords = nullptr;
		const Data::DescriptorSetNestedBindingMetaRecord* nestedBindingRecords = nullptr;

	public:
		PipelineLayoutMetadataBlobReader() = default;
		~PipelineLayoutMetadataBlobReader() = default;

		bool open(const void* data, uint32 size);

		uint32 getSourceHash() const { return subHeader->sourceHash; }
		uint16 getPipelineBindingCount() const { return subHeader->pipelineBindingCount; }
		bool isDescriptorSetBinding(uint16 pipelingBindingIndex) const;
		uint16 getPipelineBindingShaderRegister(uint16 pipelingBindingIndex) const;
		uint16 getDescriptorSetNestedBindingCount(uint16 pipelingBindingIndex) const;
		DescriptorSetNestedBindingMetaInfo getDescriptorSetNestedBinding(uint16 pipelingBindingIndex, uint16 descriptorSetNestedBindingIndex) const;
	};
}
