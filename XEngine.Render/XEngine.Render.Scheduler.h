#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Containers.ArrayList.h> // TODO: Remove

#include <XEngine.GfxHAL.D3D12.h>

#include "XEngine.Render.Allocation.h"

// NOTE: This is dummy "placeholder" implementation just to get interface working.

namespace XEngine::Render::Scheduler
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
					GfxHAL::BufferHandle bufferHandle;
				} buffer;

				struct
				{
					GfxHAL::TextureHandle textureHandle;
				} texture;

				struct
				{
					GfxHAL::ResourceViewHandle resourceViewHandle;
				} descriptor;

				struct
				{
					GfxHAL::RenderTargetViewHandle renderTargetViewHandle;
				} renderTarget;

				struct
				{
					GfxHAL::DepthStencilViewHandle depthStencilViewHandle;
				} depthRenderTarget;
			};
		};

	private:
		GfxHAL::Device* device = nullptr;
		GfxHAL::DeviceMemoryAllocationHandle deviceMemoryPool = GfxHAL::DeviceMemoryAllocationHandle::Zero;
		uint64 deviceMemoryPoolSize = 0;

		//Entry* entries = nullptr;
		//uint16 entryCount = 0;
		XLib::ArrayList<Entry> entries;

	public:
		TransientResourcePool() = default;
		~TransientResourcePool() = default;

		void initialize(GfxHAL::Device& device, GfxHAL::DeviceMemoryAllocationHandle deviceMemoryPool, uint64 deviceMemoryPoolSize);

		// NOTE: Temporary
		void reset();

		//inline GfxHAL::DeviceMemoryAllocationHandle getMemoryPool() const { return deviceMemoryPool; }
	};

	class PassExecutionContext : public XLib::NonCopyable
	{
		friend Schedule;

	private:
		GfxHAL::Device& device;
		const Schedule& schedule;
		TransientResourcePool& transientResourcePool;
		TransientDescriptorAllocator& transientDescriptorAllocator;
		TransientUploadMemoryAllocator& transientUploadMemoryAllocator;

	private:
		inline PassExecutionContext(GfxHAL::Device& device,
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
		GfxHAL::DescriptorAddress allocateTransientDescriptors(uint16 descriptorCount);
		GfxHAL::DescriptorSetReference allocateTransientDescriptorSet(GfxHAL::DescriptorSetLayoutHandle descriptorSetLayout);
		UploadMemoryAllocationInfo allocateTransientUploadMemory(uint32 size); // Aligned up to `Device::ConstantBufferBindAlignment`.

		GfxHAL::BufferHandle resolveBuffer(Scheduler::BufferHandle buffer);
		GfxHAL::TextureHandle resolveTexture(Scheduler::TextureHandle texture);

		GfxHAL::ResourceViewHandle createTransientBufferView(GfxHAL::BufferHandle buffer);
		GfxHAL::ResourceViewHandle createTransientTextureView(GfxHAL::TextureHandle texture,
			GfxHAL::TexelViewFormat format = GfxHAL::TexelViewFormat::Undefined,
			bool writable = false, const GfxHAL::TextureSubresourceRange& subresourceRange = {});
		GfxHAL::RenderTargetViewHandle createTransientRenderTargetView(GfxHAL::TextureHandle texture,
			GfxHAL::TexelViewFormat format, uint8 mipLevel = 0, uint16 arrayIndex = 0);
		GfxHAL::DepthStencilViewHandle createTransientDepthStencilView(GfxHAL::TextureHandle texture,
			bool writableDepth, bool writableStencil, uint8 mipLevel = 0, uint16 arrayIndex = 0);
	};

	using PassExecutorFunc = void(*)(PassExecutionContext& executionBroker,
		GfxHAL::Device& device, GfxHAL::CommandList& commandList, const void* userData);

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
			GfxHAL::ResourceType type;
			ResourceLifetime lifetime;

			union
			{
				uint32 transientBufferSize;
				GfxHAL::TextureDesc transientTextureDesc;
				GfxHAL::BufferHandle externalBufferHandle;
				GfxHAL::TextureHandle externalTextureHandle;
			};

			uint32 transientResourcePoolEntryIndex;
		};

		struct Pass
		{
			PassExecutorFunc executporFunc;
			const void* userData;
		};

	private:
		GfxHAL::Device* device = nullptr;
		GfxHAL::CommandList commandList;

		XLib::ArrayList<Resource> resources;
		XLib::ArrayList<Pass> passes;

	public:
		Schedule() = default;
		~Schedule() = default;

		void initialize(GfxHAL::Device& device);

		BufferHandle createTransientBuffer(uint32 size);
		TextureHandle createTransientTexture(const GfxHAL::TextureDesc& textureDesc);

		BufferHandle importExternalBuffer(GfxHAL::BufferHandle buffer);
		TextureHandle importExternalTexture(GfxHAL::TextureHandle texture);

		void* allocateUserData(uint32 size);

		//void createVirtualCPUResource();

		void addPass(PassType type, const PassDependenciesDesc& dependencies,
			PassExecutorFunc executorFunc, const void* userData);

		void compile();

		uint64 getTransientResourcePoolMemoryRequirement() const;

		void execute(TransientResourcePool& transientResourcePool,
			TransientDescriptorAllocator& transientDescriptorAllocator,
			TransientUploadMemoryAllocator& transientUploadMemoryAllocator);
	};
}
