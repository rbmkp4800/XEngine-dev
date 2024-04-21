#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

#include "XEngine.Gfx.Allocation.h"

// TODO: We may avoid using super-long split-barriers via just monitoring ongoing sync. If we do same sync that will cover our barrier, we flush our ending-barrier.

namespace XEngine::Gfx::Scheduler
{
	class Graph;
	class PassExecutionContext;

	enum class BufferHandle : uint32 {};
	enum class TextureHandle : uint32 {};

	enum class PassType : uint8
	{
		None = 0,
		Graphics,
		Compute,
		Copy,
	};

	using PassExecutorFunc = void(*)(PassExecutionContext& executionBroker,
		HAL::Device& device, HAL::CommandList& commandList, const void* userData);

	class TransientResourcePool : public XLib::NonCopyable
	{
		friend Graph;
		friend PassExecutionContext;

	private:
		enum class EntryType : uint8;

		struct Entry;

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

		//inline HAL::DeviceMemoryAllocationHandle getMemoryPool() const { return deviceMemoryPool; }
	};

	class PassExecutionContext final : public XLib::NonCopyable
	{
		friend Graph;

	private:
		HAL::Device& device;
		const Graph& graph;
		HAL::DescriptorAllocatorHandle descriptorAllocator;
		TransientUploadMemoryAllocator& transientUploadMemoryAllocator;
		TransientResourcePool& transientResourcePool;

	private:
		PassExecutionContext(HAL::Device& device, const Graph& graph,
			HAL::DescriptorAllocatorHandle descriptorAllocator,
			TransientUploadMemoryAllocator& transientUploadMemoryAllocator,
			TransientResourcePool& transientResourcePool);
		~PassExecutionContext() = default;

	public:
		HAL::DescriptorSet allocateDescriptorSet(HAL::DescriptorSetLayoutHandle descriptorSetLayout);
		UploadMemoryAllocationInfo allocateUploadMemory(uint32 size);

		HAL::BufferHandle resolveBuffer(BufferHandle buffer);
		HAL::TextureHandle resolveTexture(TextureHandle texture);
	};

	// class ResourceAccessCollector?
	class PassDependencyCollector final : public XLib::NonCopyable
	{
		friend Graph;

	private:
		Graph* graph = nullptr;
		uint16 dependencyCount = 0;
		uint16 magic = 0;

	public:
		PassDependencyCollector() = default;
		~PassDependencyCollector();

		//void addVertexOrIndexBufferRead(BufferHandle bufferHandle);
		//void addConstantBufferRead();
		//void addIndirectArgumentBufferRead();
		//void addRaytracingAccelerationStructureBufferRead();

		void addBufferShaderRead(BufferHandle bufferHandle);
		void addBufferShaderWrite(BufferHandle bufferHandle, BufferHandle& modifiedBufferHandle, bool dependsOnPreviousShaderWrite = true);

		void addTextureShaderRead(TextureHandle textureHandle, const HAL::TextureSubresourceRange* subresourceRange = nullptr);
		void addTextureShaderWrite(TextureHandle textureHandle, TextureHandle& modifiedTextureHandle, const HAL::TextureSubresourceRange* subresourceRange = nullptr, bool dependsOnPreviousShaderWrite = true);

		void addColorRenderTarget(TextureHandle textureHandle, TextureHandle& modifiedTextureHandle, uint8 mipLevel = 0, uint16 arrayIndex = 0);
		void addDepthStencilRenderTarget(TextureHandle textureHandle, TextureHandle& modifiedTextureHandle, uint8 mipLevel = 0, uint16 arrayIndex = 0);
		void addDepthStencilRenderTargetReadOnly(TextureHandle textureHandle, TextureHandle& modifiedTextureHandle, uint8 mipLevel = 0, uint16 arrayIndex = 0);

		//void addSideEffect();
	};

	class Graph final : public XLib::NonCopyable
	{
		friend PassExecutionContext;
		friend PassDependencyCollector;

	private:
		static constexpr uint16 MaxResourceCountLog2 = 10;
		static constexpr uint16 MaxPassCountLog2 = 10;
		static constexpr uint16 MaxDenendencyCountLog2 = 12;

		static constexpr uint16 MaxResourceCount = 1 << MaxResourceCountLog2;
		static constexpr uint16 MaxPassCount = 1 << MaxPassCountLog2;
		static constexpr uint16 MaxDenendencyCount = 1 << MaxDenendencyCountLog2;

		enum class ResourceOrigin : uint8;
		enum class ResourceAccessType : uint8;

		struct Resource;
		struct Pass;
		struct Dependency; // Access?? Same resource-pass pair can have multiple common items (subresource access) so it seems that `Access` is more appropriate.

		static inline bool IsModifyingAccess(ResourceAccessType access);

	private:
		HAL::Device* device = nullptr;
		byte* poolsMemory = nullptr;

		Resource* resources = nullptr;
		Pass* passes = nullptr;
		Dependency* dependencies = nullptr;
		byte* userDataPool = nullptr;

		uint16 resourcePoolSize = 0;
		uint16 passPoolSize = 0;
		uint16 dependencyPoolSize = 0;

		uint16 resourceCount = 0;
		uint16 passCount = 0;
		uint16 dependencyCount = 0;

		uint32 userDataPoolSize = 0;
		uint32 userDataAllocatedSize = 0;

		PassDependencyCollector* issuedPassDependencyCollector = nullptr;
		uint16 issuedPassDependencyCollectorMagic = 0;

	public:
		Graph() = default;
		inline ~Graph() { destroy(); }

		void initialize(HAL::Device& device, uint16 resourcePoolSize, uint16 passPoolSize, uint16 dependencyPoolSize, uint32 userDataPoolSize);
		void destroy();

		BufferHandle createTransientBuffer(uint32 size);
		TextureHandle createTransientTexture(const HAL::TextureDesc& textureDesc);
		BufferHandle importExternalBuffer(HAL::BufferHandle buffer);
		TextureHandle importExternalTexture(HAL::TextureHandle texture, HAL::TextureLayout defaultLayout);
		//void createVirtualCPUResource();

		void* allocateUserData(uint32 size);

		void issuePassDependencyCollector(PassDependencyCollector& dependencyCollector);
		void addPass(PassType type, PassDependencyCollector& dependencyCollector,
			PassExecutorFunc executorFunc, const void* userData);

		void addBufferShaderReadWriteBarrier(BufferHandle bufferHandle);
		void addTextureShaderReadWriteBarrier(TextureHandle textureHandle);

		void compile();

		uint64 getTransientResourcePoolMemoryRequirement() const;

		void execute(HAL::CommandAllocatorHandle commandAllocator, HAL::DescriptorAllocatorHandle descriptorAllocator,
			TransientUploadMemoryAllocator& transientUploadMemoryAllocator, TransientResourcePool& transientResourcePool);
	};
}
