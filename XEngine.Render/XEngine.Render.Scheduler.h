#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.D3D12.h>

#include "XEngine.Render.Allocation.h"

// NOTE: This is dummy "placeholder" implementation just to get interface working.

namespace XEngine::Render::Scheduler
{
	enum class BufferHandle : uint32 {};
	enum class TextureHandle : uint32 {};

	class TransientResourcePool : public XLib::NonCopyable
	{
	private:
		enum class EntryType : uint8;

		struct Entry;

	private:
		HAL::MemoryBlockHandle deviceMemoryPool = HAL::MemoryBlockHandle::Zero;
		uint64 deviceMemoryPoolSize = 0;

		Entry* entries = nullptr;
		uint16 entryCount = 0;

	public:
		TransientResourcePool() = default;
		~TransientResourcePool() = default;

		void initialize(HAL::MemoryBlockHandle deviceMemoryPool, uint64 deviceMemoryPoolSize);

		// NOTE: Temporary
		void reset();

		//inline HAL::MemoryBlockHandle getMemoryPool() const { return deviceMemoryPool; }
	};

	class PassExecutionBroker : public XLib::NonCopyable
	{
		friend class Pipeline;

	private:
		HAL::Device& device;
		TransientResourcePool& transientResourcePool;
		CircularDescriptorAllocator& transientDescriptorAllocator;
		CircularUploadMemoryAllocator& transientUploadMemoryAllocator;

	private:
		PassExecutionBroker() = default;
		~PassExecutionBroker() = default;

	public:
		HAL::DescriptorAddress allocateTransientDescriptors(uint16 descriptorCount);
		HAL::DescriptorSetReference allocateTransientDescriptorSet(HAL::DescriptorSetLayoutHandle descriptorSetLayout);
		UploadMemoryAllocationInfo allocateTransientUploadMemory(uint32 size); // Aligned up to `Host::Device::ConstantBufferBindAlignment`.

		HAL::BufferHandle resolveBuffer(Scheduler::BufferHandle buffer);
		HAL::TextureHandle resolveTexture(Scheduler::TextureHandle texture);

		HAL::ResourceViewHandle createTransientBufferView(Scheduler::BufferHandle buffer);
		HAL::ResourceViewHandle createTransientTextureView(Scheduler::TextureHandle texture,
			HAL::TexelViewFormat format, bool writable, const HAL::TextureSubresourceRange& subresourceRange);
		HAL::RenderTargetViewHandle createTransientRenderTargetView(Scheduler::TextureHandle texture,
			HAL::TexelViewFormat format, uint8 mipLevel = 0, uint16 arrayIndex = 0);
		HAL::DepthStencilViewHandle createTransientDepthStencilView(Scheduler::TextureHandle texture,
			bool writableDepth, bool writableStencil, uint8 mipLevel = 0, uint16 arrayIndex = 0);
	};

	using PassExecutorFunc = void(*)(PassExecutionBroker& executionBroker,
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

	class Pipeline : public XLib::NonCopyable
	{
	private:
		enum class ResourceLifetime : uint8;

		struct Resource;
		struct Pass;

	private:
		HAL::Device* device = nullptr;
		HAL::CommandList commandList;

		Resource* resources = nullptr;
		Pass* passes = nullptr;

		uint32 resourceCount = 0;
		uint32 passCount = 0;

	public:
		Pipeline() = default;
		~Pipeline() = default;

		void reset(HAL::Device& device);

		BufferHandle createTransientBuffer(uint32 size);
		TextureHandle createTransientTexture(const HAL::TextureDesc& textureDesc);

		BufferHandle importExternalBuffer(HAL::BufferHandle buffer);
		TextureHandle importExternalTexture(HAL::TextureHandle texture);

		void* allocateUserData(uint32 size);

		//void createVirtualCPUResource();

		void schedulePass(PassType type, const PassDependenciesDesc& dependencies,
			PassExecutorFunc executorFunc, const void* userData);

		void compile();

		uint64 getTransientResourcePoolMemoryRequirement() const;

		void execute(TransientResourcePool& transientResourcePool,
			CircularDescriptorAllocator& transientDescriptorAllocator,
			CircularUploadMemoryAllocator& transientUploadMemoryAllocator);
	};
}
