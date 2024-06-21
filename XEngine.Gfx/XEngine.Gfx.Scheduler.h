#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

#include "XEngine.Gfx.Allocation.h"

// TODO: Split barriers?

namespace XEngine::Gfx::Scheduler
{
	class Graph;
	class GraphExecutionContext;

	enum class BufferHandle : uint32 {};
	enum class TextureHandle : uint32 {};

	enum class PassType : uint8
	{
		None = 0,
		Graphics,
		Compute,
		Copy,
	};

	enum class ShaderStage : uint8
	{
		Any = 0,
		Compute,
		Graphics,
		PrePixel,
		Pixel,
	};

	using PassExecutorFunc = void(*)(GraphExecutionContext& executionBroker,
		HAL::Device& device, HAL::CommandList& commandList, const void* userData);

	class TransientResourceCache : public XLib::NonCopyable
	{
		friend Graph;
		friend GraphExecutionContext;

	private:
		enum class EntryType : uint8;

		struct Entry;

		static constexpr uint32 AllocationAlignment = 0x10000;

	private:
		HAL::Device* device = nullptr;
		HAL::DeviceMemoryAllocationHandle deviceMemoryPool = HAL::DeviceMemoryAllocationHandle::Zero;
		uint64 deviceMemoryPoolSize = 0;

		Entry* entries = nullptr;
		uint16 entryCount = 0;

	public:
		TransientResourceCache() = default;
		~TransientResourceCache() = default;

		void initialize(HAL::Device& device, HAL::DeviceMemoryAllocationHandle deviceMemoryPool, uint64 deviceMemoryPoolSize);

		//inline HAL::DeviceMemoryAllocationHandle getMemoryPool() const { return deviceMemoryPool; }
	};

	class GraphExecutionContext final : public XLib::NonCopyable
	{
		friend Graph;

	private:
		HAL::Device& device;
		const Graph& graph;
		HAL::DescriptorAllocatorHandle transientDescriptorAllocator;
		TransientUploadMemoryAllocator& transientUploadMemoryAllocator;

	private:
		GraphExecutionContext(HAL::Device& device, const Graph& graph,
			HAL::DescriptorAllocatorHandle transientDescriptorAllocator,
			TransientUploadMemoryAllocator& transientUploadMemoryAllocator);
		~GraphExecutionContext() = default;

	public:
		HAL::DescriptorSet allocateDescriptorSet(HAL::DescriptorSetLayoutHandle descriptorSetLayout);
		UploadBufferPointer allocateUploadMemory(uint32 size);

		HAL::BufferHandle resolveBuffer(BufferHandle bufferHandle);
		HAL::TextureHandle resolveTexture(TextureHandle textureHandle);
	};

	class PassDependencyCollector final : public XLib::NonCopyable
	{
		friend Graph;

	private:
		Graph* graph = nullptr;
		//uint16 magic = 0;

	//private:
	//	void addDependeny(HAL::ResourceType resourceType, uint32 resourceHandle, HAL::BarrierAccess access, HAL::BarrierSync sync);

	public:
		PassDependencyCollector() = default;
		~PassDependencyCollector();

		void addBufferAcces(BufferHandle bufferHandle,
			HAL::BarrierAccess access, HAL::BarrierSync sync);
		void addTextureAccess(TextureHandle textureHandle,
			HAL::BarrierAccess access, HAL::BarrierSync sync,
			HAL::TextureSubresourceRange* subresourceRange);

		//void addManualBufferAccess(BufferHandle bufferHandle);
		//void addManualTextureAccess(TextureHandle textureHandle);

		void addBufferShaderRead(BufferHandle bufferHandle,
			ShaderStage shaderStage = ShaderStage::Any);
		void addBufferShaderWrite(BufferHandle bufferHandle,
			ShaderStage shaderStage = ShaderStage::Any,
			bool dependsOnPrecedingShaderWrite = true);

		void addTextureShaderRead(TextureHandle textureHandle,
			const HAL::TextureSubresourceRange* subresourceRange = nullptr,
			ShaderStage shaderStage = ShaderStage::Any);
		void addTextureShaderWrite(TextureHandle textureHandle,
			const HAL::TextureSubresourceRange* subresourceRange = nullptr,
			ShaderStage shaderStage = ShaderStage::Any,
			bool dependsOnPrecedingShaderWrite = true);

		void addColorRenderTarget(TextureHandle textureHandle,
			uint8 mipLevel = 0, uint16 arrayIndex = 0);
		void addDepthStencilRenderTarget(TextureHandle textureHandle,
			uint8 mipLevel = 0, uint16 arrayIndex = 0);
		void addDepthStencilRenderTargetReadOnly(TextureHandle textureHandle,
			uint8 mipLevel = 0, uint16 arrayIndex = 0);

		void close();
	};

	struct GraphSettings
	{
		uint16 resourcePoolSize;
		uint16 passPoolSize;
		uint16 dependencyPoolSize;
		uint16 barrierPoolSize;
		uint32 userDataPoolSize;
	};

	class Graph final : public XLib::NonCopyable
	{
		friend GraphExecutionContext;
		friend PassDependencyCollector;

	private:
		static constexpr uint16 MaxResourceCountLog2 = 10;
		static constexpr uint16 MaxPassCountLog2 = 10;
		static constexpr uint16 MaxDenendencyCountLog2 = 12;

		static constexpr uint16 MaxResourceCount = 1 << MaxResourceCountLog2;
		static constexpr uint16 MaxPassCount = 1 << MaxPassCountLog2;
		static constexpr uint16 MaxDenendencyCount = 1 << MaxDenendencyCountLog2;

		struct Resource;
		struct Pass;
		struct Dependency;
		struct Barrier;

	private:
		HAL::Device* device = nullptr;
		TransientResourceCache* transientResourceCache = nullptr;

		byte* poolsMemory = nullptr;
		Resource* resources = nullptr;
		Pass* passes = nullptr;
		Dependency* dependencies = nullptr;
		Barrier* barriers = nullptr;
		byte* userDataPool = nullptr;

		uint16 resourcePoolSize = 0;
		uint16 passPoolSize = 0;
		uint16 dependencyPoolSize = 0;
		uint16 barrierPoolSize = 0;

		uint16 resourceCount = 0;
		uint16 passCount = 0;
		uint16 dependencyCount = 0;
		uint16 barrierCount = 0;

		uint32 userDataPoolSize = 0;
		uint32 userDataAllocatedSize = 0;

		PassDependencyCollector* issuedPassDependencyCollector = nullptr;
		//uint16 issuedPassDependencyCollectorMagic = 0;
		bool isCompiled = false;

	public:
		Graph() = default;
		inline ~Graph() { destroy(); }

		void initialize(HAL::Device& device,
			TransientResourceCache& transientResourceCache,
			const GraphSettings& settings);
		void destroy();

		BufferHandle createTransientBuffer(uint32 size, uint64 nameXSH);
		TextureHandle createTransientTexture(HAL::TextureDesc textureDesc, uint64 nameXSH);
		BufferHandle importExternalBuffer(HAL::BufferHandle bufferHandle);
		TextureHandle importExternalTexture(HAL::TextureHandle textureHandle, HAL::TextureLayout defaultLayout);
		//void createVirtualCPUResource();

		void* allocateUserData(uint32 size);

		void addPass(PassType type, PassDependencyCollector& dependencyCollector,
			PassExecutorFunc executorFunc, const void* userData);

		void compile();

		void execute(HAL::CommandAllocatorHandle commandAllocator,
			HAL::DescriptorAllocatorHandle transientDescriptorAllocator,
			TransientUploadMemoryAllocator& transientUploadMemoryAllocator) const;

		void reset();
	};
}
