#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Containers.ArrayList.h> // TODO: Remove

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
			EntryType type;

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

				struct
				{
					HAL::ResourceViewHandle handle;
				} resourceView;

				struct
				{
					HAL::ColorRenderTargetHandle handle;
				} colorRenderTarget;

				struct
				{
					HAL::DepthStencilRenderTargetHandle handle;
				} depthStencilRenderTarget;
			};
		};

	private:
		HAL::Device* device = nullptr;
		HAL::DeviceMemoryAllocationHandle deviceMemoryPool = HAL::DeviceMemoryAllocationHandle::Zero;
		uint64 deviceMemoryPoolSize = 0;

		//Entry* entries = nullptr;
		//uint16 entryCount = 0;
		XLib::ArrayList<Entry> entries;

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
		TransientResourcePool& transientResourcePool;
		TransientDescriptorAllocator& transientDescriptorAllocator;
		TransientUploadMemoryAllocator& transientUploadMemoryAllocator;

	private:
		inline PassExecutionContext(HAL::Device& device,
			const Schedule& schedule,
			TransientResourcePool& transientResourcePool,
			TransientDescriptorAllocator& transientDescriptorAllocator,
			TransientUploadMemoryAllocator& transientUploadMemoryAllocator) :
			device(device),
			schedule(schedule),
			transientResourcePool(transientResourcePool),
			transientDescriptorAllocator(transientDescriptorAllocator),
			transientUploadMemoryAllocator(transientUploadMemoryAllocator)
		{ }
		~PassExecutionContext() = default;

	public:
		HAL::DescriptorAddress allocateTransientDescriptors(uint16 descriptorCount);
		HAL::DescriptorSetReference allocateTransientDescriptorSet(HAL::DescriptorSetLayoutHandle descriptorSetLayout);
		UploadMemoryAllocationInfo allocateTransientUploadMemory(uint32 size); // Aligned up to `Device::ConstantBufferBindAlignment`.

		HAL::BufferHandle resolveBuffer(Scheduler::BufferHandle buffer);
		HAL::TextureHandle resolveTexture(Scheduler::TextureHandle texture);

		HAL::ResourceViewHandle createTransientBufferView(HAL::BufferHandle buffer);
		HAL::ResourceViewHandle createTransientTextureView(HAL::TextureHandle texture,
			HAL::TexelViewFormat format = HAL::TexelViewFormat::Undefined,
			bool writable = false, const HAL::TextureSubresourceRange& subresourceRange = {});
		HAL::ColorRenderTargetHandle createTransientColorRenderTarget(HAL::TextureHandle texture,
			HAL::TexelViewFormat format, uint8 mipLevel = 0, uint16 arrayIndex = 0);
		HAL::DepthStencilRenderTargetHandle createTransientDepthStencilRenderTarget(HAL::TextureHandle texture,
			bool writableDepth, bool writableStencil, uint8 mipLevel = 0, uint16 arrayIndex = 0);
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

#if 0

	struct PassDependencyDesc
	{
		PassDependencyType type;

		union
		{
			struct
			{
				PipelineBufferHandle buffer;
				BufferAccess access;
			} buffer;

			struct
			{
				PipelineTextureHandle texture;
				TextureAccess access;
			} texture;
		};
	};

#else

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

#endif

	enum class PassType : uint8
	{
		None = 0,
		Graphics,
		Compute,
		Copy,
	};

	/*class PipelineCommandListPool : public XLib::NonCopyable
	{

	};*/

	class Schedule : public XLib::NonCopyable
	{
		friend PassExecutionContext;

	private:
		enum class ResourceLifetime : uint8
		{
			Undefined = 0,
			Transient,
			External,
		};

		struct Resource
		{
			HAL::ResourceType type;
			ResourceLifetime lifetime;

			union
			{
				uint32 transientBufferSize;
				HAL::TextureDesc transientTextureDesc;
				HAL::BufferHandle externalBufferHandle;
				HAL::TextureHandle externalTextureHandle;
			};

			uint32 transientResourcePoolEntryIndex;
		};

		struct Pass
		{
			PassExecutorFunc executporFunc;
			const void* userData;
		};

	private:
		HAL::Device* device = nullptr;

		XLib::ArrayList<Resource> resources;
		XLib::ArrayList<Pass> passes;

	public:
		Schedule() = default;
		~Schedule() = default;

		void initialize(HAL::Device& device);

		BufferHandle createTransientBuffer(uint32 size);
		TextureHandle createTransientTexture(const HAL::TextureDesc& textureDesc);

		BufferHandle importExternalBuffer(HAL::BufferHandle buffer);
		TextureHandle importExternalTexture(HAL::TextureHandle texture);

		void* allocateUserData(uint32 size);

		//void createVirtualCPUResource();

		void addPass(PassType type, const PassDependenciesDesc& dependencies,
			PassExecutorFunc executorFunc, const void* userData);

		void compile();

		uint64 getTransientResourcePoolMemoryRequirement() const;

		void execute(HAL::CommandAllocatorHandle commandAllocator,
			TransientResourcePool& transientResourcePool,
			TransientDescriptorAllocator& transientDescriptorAllocator,
			TransientUploadMemoryAllocator& transientUploadMemoryAllocator);
	};
}
