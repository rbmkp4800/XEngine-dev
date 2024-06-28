#pragma once

#include <XLib.h>

#include "XEngine.Gfx.HAL.Shared.h"

// TODO: Probably we should remove all pseudo-error-reporting via `return false` from readers and just do master assert?
// TODO: `uint16` binding count probably should be swapped to `uint8` everywhere.

namespace XEngine::Gfx::HAL::BlobFormat::Data
{
	constexpr uint32 GenericBlobSignature				= 0x874C2131;
	constexpr uint16 DescriptorSetLayoutBlobSignature	= 0xF1A6;
	constexpr uint16 PipelineBindingLayoutBlobSignature	= 0xE1B2;
	constexpr uint16 ShaderBlobSignature				= 0xE084;

	struct GenericBlobHeader // 16 bytes
	{
		uint32 genericSignature;
		uint32 size;
		uint32 checksum; // CRC-32/zlib
		uint16 specificSignature;
		uint16 version;
	};
	static_assert(sizeof(GenericBlobHeader) == 16);

	struct DescriptorSetLayoutBlobSubHeader // 8 bytes
	{
		uint32 hash;
		uint16 bindingCount;
		uint16 _padding0;
	};
	static_assert(sizeof(DescriptorSetLayoutBlobSubHeader) == 8);

	struct DescriptorSetBindingRecord // 12 bytes
	{
		uint32 nameXSH0;
		uint32 nameXSH1;
		uint16 descriptorCount;
		DescriptorType descriptorType;
	};
	static_assert(sizeof(DescriptorSetBindingRecord) == 12);

	struct PipelineBindingLayoutBlobSubHeader // 8 bytes
	{
		uint32 hash;
		uint16 bindingCount;
		uint16 platformDataSize;
	};
	static_assert(sizeof(PipelineBindingLayoutBlobSubHeader) == 8);

	struct PipelineBindingRecord // 16 bytes
	{
		// [0..8)
		uint64 nameXSH;
		// [8..12)
		uint32 descriptorSetLayoutHash;
		// [12..14)
		uint16 d3dRootParameterIndex;
		// [14..15)
		PipelineBindingType type;
		// [15..16)
		uint8 inplaceConstantCount;
	};
	static_assert(sizeof(PipelineBindingRecord) == 16);

	struct ShaderBlobSubHeader // 8 bytes
	{
		uint32 pipelineBindingLayoutHash;
		uint16 bytecodeSizeLo16;
		uint8 bytecodeSizeHi8;
		ShaderType shaderType;
	};
	static_assert(sizeof(ShaderBlobSubHeader) == 8);
}

namespace XEngine::Gfx::HAL::BlobFormat
{
	// Descriptor set layout blob //////////////////////////////////////////////////////////////////

	struct DescriptorSetLayoutBlobInfo
	{
		uint16 bindingCount;
		uint32 hash;
	};

	struct DescriptorSetBindingInfo
	{
		uint64 nameXSH;
		uint16 descriptorCount;
		DescriptorType descriptorType;
	};

	class DescriptorSetLayoutBlobWriter
	{
	private:
		DescriptorSetLayoutBlobInfo blobInfo = {};
		uint32 blobSize = 0;

		void* blobMemory = nullptr;

		Data::DescriptorSetLayoutBlobSubHeader* subHeader = nullptr;
		Data::DescriptorSetBindingRecord* bindingRecords = nullptr;

	public:
		DescriptorSetLayoutBlobWriter() = default;
		~DescriptorSetLayoutBlobWriter() = default;

		void setup(const DescriptorSetLayoutBlobInfo& blobInfo);

		uint32 getBlobSize() const { return blobSize; }
		void setBlobMemory(void* memory, uint32 memorySize);

		void writeBindingInfo(uint16 bindingIndex, const DescriptorSetBindingInfo& bindingInfo);
		void finalize();
	};

	class DescriptorSetLayoutBlobReader
	{
	private:
		const Data::DescriptorSetLayoutBlobSubHeader* subHeader = nullptr;
		const Data::DescriptorSetBindingRecord* bindingRecords = nullptr;

	public:
		DescriptorSetLayoutBlobReader() = default;
		~DescriptorSetLayoutBlobReader() = default;

		bool open(const void* data, uint32 size);
		DescriptorSetLayoutBlobInfo getBlobInfo() const;
		DescriptorSetBindingInfo getBindingInfo(uint32 bindingIndex) const;
	};


	// Pipeline binding layout blob ////////////////////////////////////////////////////////////////

	struct PipelineBindingLayoutBlobInfo
	{
		uint16 bindingCount;
		uint32 platformDataSize;
		uint32 hash;
	};

	struct PipelineBindingInfo
	{
		uint64 nameXSH;
		uint16 d3dRootParameterIndex;
		PipelineBindingType type;

		union
		{
			uint8 inplaceConstantCount;
			uint32 descriptorSetLayoutHash;
		};
	};

	class PipelineBindingLayoutBlobWriter
	{
	private:
		PipelineBindingLayoutBlobInfo blobInfo = {};
		uint32 blobSize = 0;

		void* blobMemory = nullptr;

		Data::PipelineBindingLayoutBlobSubHeader* subHeader = nullptr;
		Data::PipelineBindingRecord* bindingRecords = nullptr;
		void* platformData = nullptr;

	public:
		PipelineBindingLayoutBlobWriter() = default;
		~PipelineBindingLayoutBlobWriter() = default;

		void setup(const PipelineBindingLayoutBlobInfo& blobInfo);

		uint32 getBlobSize() const { return blobSize; }
		void setBlobMemory(void* memory, uint32 memorySize);

		void writeBindingInfo(uint16 bindingIndex, const PipelineBindingInfo& bindingInfo);
		void writePlatformData(const void* data, uint32 size);
		void finalize();
	};

	class PipelineBindingLayoutBlobReader
	{
	private:
		const Data::PipelineBindingLayoutBlobSubHeader* subHeader = nullptr;
		const Data::PipelineBindingRecord* bindingRecords = nullptr;
		const void* platformData = nullptr;

	public:
		PipelineBindingLayoutBlobReader() = default;
		~PipelineBindingLayoutBlobReader() = default;

		bool open(const void* data, uint32 size);
		PipelineBindingLayoutBlobInfo getBlobInfo() const;
		PipelineBindingInfo getBindingInfo(uint16 bindingIndex) const;
		const void* getPlatformDataPtr() const { return platformData; }
	};


	// Shader blob /////////////////////////////////////////////////////////////////////////////////

	struct ShaderBlobInfo
	{
		uint32 pipelineBindingLayoutHash;
		uint32 bytecodeSize;
		ShaderType shaderType;
	};

	class ShaderBlobWriter
	{
	private:
		ShaderBlobInfo blobInfo = {};
		uint32 blobSize = 0;

		void* blobMemory = nullptr;

		Data::ShaderBlobSubHeader* subHeader = nullptr;
		void* bytecode = nullptr;

	public:
		ShaderBlobWriter() = default;
		~ShaderBlobWriter() = default;

		void setup(const ShaderBlobInfo& blobInfo);

		uint32 getBlobSize() const { return blobSize; }
		void setBlobMemory(void* memory, uint32 memorySize);

		void writeBytecode(const void* data, uint32 size);
		void finalize();
	};

	class ShaderBlobReader
	{
	private:
		const Data::ShaderBlobSubHeader* subHeader = nullptr;
		const void* bytecode = nullptr;

	public:
		ShaderBlobReader() = default;
		~ShaderBlobReader() = default;

		bool open(const void* data, uint32 size);

		ShaderBlobInfo getBlobInfo() const;
		const void* getBytecodePtr() const { return bytecode; }
	};
}
