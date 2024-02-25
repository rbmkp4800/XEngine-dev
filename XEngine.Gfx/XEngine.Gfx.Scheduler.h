#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

#include "XEngine.Gfx.Allocation.h"

// NOTE: This is dummy "placeholder" implementation just to get interface working.

namespace XEngine::Gfx::Scheduler
{
	class Schedule;
	class PassExecutionContext;

	enum class BufferHandle : uint32 {};
	enum class TextureHandle : uint32 {};

	class TransientResourcePool : public XLib::NonCopyable
	{
		friend Schedule;
		friend PassExecutionContext;

	private:
		enum class EntryType : uint8;

		struct Entry
		{
			union
			{
				struct
				{
					HAL::BufferHandle handle;
				} buffer;

				struct
				{
					HAL::TextureHandle handle;
				} texture;
			};

			EntryType type;
		};

	private:
		HAL::Device* device = nullptr;
		HAL::DeviceMemoryAllocationHandle deviceMemoryPool = HAL::DeviceMemoryAllocationHandle::Zero;
		uint64 deviceMemoryPoolSize = 0;

		Entry* entries = nullptr;
		uint16 entryCount = 0;

	public:
		TransientResourcePool() = default;
		~TransientResourcePool() = default;

		void initialize(HAL::Device& device, HAL::DeviceMemoryAllocationHandle deviceMemoryPool, uint64 deviceMemoryPoolSize);

		// NOTE: Temporary
		void reset();

		//inline HAL::DeviceMemoryAllocationHandle getMemoryPool() const { return deviceMemoryPool; }
	};

	class PassExecutionContext : public XLib::NonCopyable
	{
		friend Schedule;

	private:
		HAL::Device& device;
		const Schedule& schedule;
		HAL::DescriptorAllocatorHandle transientDescriptorAllocator;
		TransientUploadMemoryAllocator& transientUploadMemoryAllocator;
		TransientResourcePool& transientResourcePool;

	private:
		PassExecutionContext(HAL::Device& device, const Schedule& schedule,
			HAL::DescriptorAllocatorHandle transientDescriptorAllocator,
			TransientUploadMemoryAllocator& transientUploadMemoryAllocator,
			TransientResourcePool& transientResourcePool);
		~PassExecutionContext() = default;

	public:
		HAL::DescriptorSet allocateDescriptorSet(HAL::DescriptorSetLayoutHandle descriptorSetLayout);
		UploadMemoryAllocationInfo allocateUploadMemory(uint32 size);

		HAL::BufferHandle resolveBuffer(Scheduler::BufferHandle buffer);
		HAL::TextureHandle resolveTexture(Scheduler::TextureHandle texture);
	};

	using PassExecutorFunc = void(*)(PassExecutionContext& executionBroker,
		HAL::Device& device, HAL::CommandList& commandList, const void* userData);

	enum class BufferAccess : uint8
	{
		Undefined = 0,
		ShaderRead,
		ShaderWrite,
		ShaderReadWrite,
	};

	enum class TextureAccess : uint8
	{
		Undefined = 0,
		ShaderRead,
		ShaderWrite,
		ShaderReadWrite,
		RenderTarget,
		DepthStencilReadOnly,
		DepthStencilReadWrite,
	};

	enum class PassDependencyType : uint8
	{
		Undefined = 0,
		Buffer,
		Texture,
		//ExternalCPUExecutionKickOffSignal,
		//ExternalGPUExecutionKickOffSignal,
		//VirtualCPUResource,
	};

	struct PassBufferDependencyDesc
	{
		BufferHandle buffer;
		BufferAccess access;
	};

	struct PassTextureDependencyDesc
	{
		TextureHandle texture;
		TextureAccess access;
	};

	struct PassDependenciesDesc
	{
		const PassBufferDependencyDesc* bufferDependencies;
		const PassTextureDependencyDesc* textureDependencies;
		uint8 bufferDependencyCount;
		uint8 textureDependencyCount;
	};

	enum class PassType : uint8
	{
		None = 0,
		Graphics,
		Compute,
		Copy,
	};

	class Schedule : public XLib::NonCopyable
	{
		friend PassExecutionContext;

	private:
		enum class ResourceLifetime : uint8;
		struct Resource;
		struct Pass;

	private:
		HAL::Device* device = nullptr;

		Resource* resources = nullptr;
		Pass* passes = nullptr;

	public:
		Schedule() = default;
		~Schedule() = default;

		void initialize(HAL::Device& device, uint16 maxResourceCount, uint16 maxPassCount);

		BufferHandle createTransientBuffer(uint32 size);
		TextureHandle createTransientTexture(const HAL::TextureDesc& textureDesc);
		BufferHandle importExternalBuffer(HAL::BufferHandle buffer);
		TextureHandle importExternalTexture(HAL::TextureHandle texture, HAL::TextureLayout defaultLayout);
		//void createVirtualCPUResource();

		void* allocateUserData(uint32 size);

		void addPass(PassType type, const PassDependenciesDesc& dependencies,
			PassExecutorFunc executorFunc, const void* userData);

		void compile();

		uint64 getTransientResourcePoolMemoryRequirement() const;

		void execute(HAL::CommandAllocatorHandle commandAllocator, HAL::DescriptorAllocatorHandle transientDescriptorAllocator,
			TransientUploadMemoryAllocator& transientUploadMemoryAllocator, TransientResourcePool& transientResourcePool);
	};
}
