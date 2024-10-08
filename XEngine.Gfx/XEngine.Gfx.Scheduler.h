#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Containers.CircularQueue.h>

#include <XEngine.Gfx.HAL.D3D12.h>

#include "XEngine.Gfx.Allocation.h"

// TODO: Consider adding deferred `TaskGraph::writeDescriptor` call that accepts transient resource handles. We need this to be able to allocate and populate descriptor sets before execution.
// TODO: Use hashmap or something in `TransientResourceCache` instead of retarded linear search through all entries.
// TODO: Do not allocate transient resources that are not accessed.
// TODO: Optimal resource aliasing.
// TODO: Proper `TransientMemoryPool` implementation with multiple allocations and ability to grow.
// TODO: Check that there is one and only one dependency between task and resource(subresource).
// TODO: Check that we do not have duplacete imported resources during `TaskGraph::closeAndCompile` (as a separate validation step).
// TODO: Implement proper `canOverlapPrecedingShaderWrite` handling.
// TODO: Implement proper handles.
// TODO: Try using "CompositeHeapAllocation" concept.

// TODO: Cleanup `TransientResourceCacheAccessSession::*` `queryBuffer` and `queryTexture`. A lot of code duplication there.
// We may try to introduce `HAL::ResourceDesc` (U64, that also encodes resource type), `HAL::Device::createResource`, `HAL::ResourceHandle`.

namespace XEngine::Gfx::Scheduler
{
	class TaskGraph;
	class TransientResourceCacheAccessSession;
	class TaskExecutionContext;

	static constexpr uint64 TransientResourceAllocationAlignment = 0x10000;

	enum class BufferHandle : uint32 {};
	enum class TextureHandle : uint32 {};

	enum class TaskType : uint8
	{
		None = 0,
		Graphics,
		Compute,
		Copy,
	};

	enum class ResourceShaderAccessStage : uint8
	{
		Any = 0,
		Compute,
		Graphics,
		PrePixel,
		Pixel,
	};

	using TaskExecutorFunc = void(*)(TaskExecutionContext& executionContext,
		HAL::Device& hwDevice, HAL::CommandList& hwCommandList, void* userData);


	/*class TransientMemoryPool : public XLib::NonCopyable
	{

	};*/


	class TransientResourceCache : public XLib::NonCopyable
	{
		friend TransientResourceCacheAccessSession;

	private:
		static constexpr uint8 SessionReleaseQueueSizeLog2 = 5;
		static constexpr uint16 SessionForceEvictAge = 64;

		struct Entry;

		struct SessionReleaseQueueElement
		{
			HAL::DeviceQueueSyncPoint hwSyncPoint;
		};
		using SessionReleaseQueue =
			XLib::InplaceCircularQueue<SessionReleaseQueueElement, SessionReleaseQueueSizeLog2, uint16>;

		static_assert(SessionForceEvictAge > SessionReleaseQueue::Capacity);

	private:
		HAL::Device* hwDevice = nullptr;
		HAL::DeviceMemoryHandle hwDeviceMemory = {};
		uint64 deviceMemorySize = 0;

		Entry* entries = nullptr;
		uint16 entryPoolSize = 0;
		uint16 entryCount = 0;

		SessionReleaseQueue sessionReleaseQueue;
		uint16 currentSessionIndex = 0;
		bool sessionIsOpen = false;

	private:
		void pruneSessionReleaseQueue();
		void prune();

	public:
		TransientResourceCache() = default;
		inline ~TransientResourceCache() { destroy(); }

		void initialize(HAL::Device& hwDevice, uint16 maxEntryCount = 1024);
		void destroy();
	};


	class TransientResourceCacheAccessSession : public XLib::NonCopyable
	{
	private:
		TransientResourceCache* cache = nullptr;

	public:
		TransientResourceCacheAccessSession() = default;
		~TransientResourceCacheAccessSession();

		void openAndPruneCache(TransientResourceCache& cache);
		void closeAndSetupSessionRelease(HAL::DeviceQueueSyncPoint hwSessionReleaseSyncPoint);

		HAL::BufferHandle queryBuffer(uint64 nameXSH, uint32 bufferSize, uint16 memoryOffset);
		HAL::TextureHandle queryTexture(uint64 nameXSH, HAL::TextureDesc hwTextureDesc, uint16 memoryOffset);

#if 0
		HAL::ResourceHandle queryResource(uint64 nameXSH, HAL::ResourceDesc hwDesc, uint16 memoryOffset);
#endif
	};


	class TaskDependencyCollector final : public XLib::NonCopyable
	{
		friend TaskGraph;

	private:
		TaskGraph* parent = nullptr;

	private:
		static inline HAL::BarrierSync TranslateSchResourceShaderAccessStageToHwBarrierSync(ResourceShaderAccessStage shaderStage);

	private:
		inline explicit TaskDependencyCollector(TaskGraph& parent);

	public:
		TaskDependencyCollector() = delete;
		inline ~TaskDependencyCollector();

		inline TaskDependencyCollector(TaskDependencyCollector&& that);
		inline void operator = (TaskDependencyCollector&& that);

		// Temporary
		inline TaskDependencyCollector& addBufferAcces(BufferHandle hwBufferHandle,
			HAL::BarrierSync hwSync, HAL::BarrierAccess hwAccess);
		inline TaskDependencyCollector& addTextureAccess(TextureHandle hwTextureHandle,
			HAL::BarrierSync hwSync, HAL::BarrierAccess hwAccess, HAL::TextureLayout hwTextureLayout,
			const HAL::TextureSubresourceRange* hwSubresourceRange);

		//void addManualBufferAccess(BufferHandle hwBufferHandle);
		//void addManualTextureAccess(TextureHandle hwTextureHandle);

		inline TaskDependencyCollector& addBufferShaderRead(BufferHandle hwBufferHandle,
			ResourceShaderAccessStage shaderStage = ResourceShaderAccessStage::Any);
		inline TaskDependencyCollector& addBufferShaderWrite(BufferHandle hwBufferHandle,
			ResourceShaderAccessStage shaderStage = ResourceShaderAccessStage::Any,
			bool dependsOnPrecedingShaderWrite = true);

		inline TaskDependencyCollector& addTextureShaderRead(TextureHandle hwTextureHandle,
			ResourceShaderAccessStage shaderStage = ResourceShaderAccessStage::Any,
			const HAL::TextureSubresourceRange* hwSubresourceRange = nullptr);
		inline TaskDependencyCollector& addTextureShaderWrite(TextureHandle hwTextureHandle,
			ResourceShaderAccessStage shaderStage = ResourceShaderAccessStage::Any,
			const HAL::TextureSubresourceRange* hwSubresourceRange = nullptr,
			bool dependsOnPrecedingShaderWrite = true);

		inline TaskDependencyCollector& addColorRenderTarget(TextureHandle hwTextureHandle/*,
			uint8 mipLevel = 0, uint16 arrayIndex = 0*/);
		inline TaskDependencyCollector& addDepthStencilRenderTarget(TextureHandle hwTextureHandle/*,
			uint8 mipLevel = 0, uint16 arrayIndex = 0*/);
		inline TaskDependencyCollector& addDepthStencilRenderTargetReadOnly(TextureHandle hwTextureHandle/*,
			uint8 mipLevel = 0, uint16 arrayIndex = 0*/);
	};


	class TaskGraph final : public XLib::NonCopyable
	{
		friend TaskExecutionContext;
		friend TaskDependencyCollector;

	private:
		static constexpr uint16 MaxResourceCountLog2 = 9;
		static constexpr uint16 MaxTaskCountLog2 = 9;
		static constexpr uint16 MaxDenendencyCountLog2 = 12;

		static constexpr uint16 MaxResourceCount = 1 << MaxResourceCountLog2;
		static constexpr uint16 MaxTaskCount = 1 << MaxTaskCountLog2;
		static constexpr uint16 MaxDenendencyCount = 1 << MaxDenendencyCountLog2;

		struct Resource;
		struct Task;
		struct Dependency;
		struct Barrier;

	private:
		void* internalMemoryBlock = nullptr;
		Resource* resources = nullptr;
		Task* tasks = nullptr;
		Dependency* dependencies = nullptr;
		Barrier* barriers = nullptr;
		void* userDataPool = nullptr;

		HAL::Device* hwDevice = nullptr;
		HAL::CommandAllocatorHandle hwCommandAllocator = {};
		HAL::DescriptorAllocatorHandle hwTransientDescriptorAllocator = {};
		CircularUploadMemoryAllocator* transientUploadMemoryAllocator = nullptr;
		TransientResourceCache* transientResourceCache = nullptr;

		uint16 resourcePoolSize = 0;
		uint16 taskPoolSize = 0;
		uint16 dependencyPoolSize = 0;
		uint16 barrierPoolSize = 0;

		uint16 resourceCount = 0;
		uint16 taskCount = 0;
		uint16 dependencyCount = 0;
		uint16 barrierCount = 0;

		uint32 userDataPoolSize = 0;
		uint32 userDataAllocatedSize = 0;

		TaskDependencyCollector* issuedTaskDependencyCollector = nullptr;
		uint16 postExecutionLocalBarrierChainHeadIdx = 0;

	private:
		void registerIssuedTaskDependenciesCollector(TaskDependencyCollector& registree);
		void relocateIssuedTaskDependenciesCollector(TaskDependencyCollector& from, TaskDependencyCollector& to);
		void revokeIssuedTaskDependenciesCollector();

		void addTaskDependency(TaskDependencyCollector& sourceTaskDependenlyCollector,
			HAL::ResourceType hwResourceType, uint32 hwResourceHandle,
			HAL::BarrierSync hwSync, HAL::BarrierAccess hwAccess, HAL::TextureLayout hwTextureLayout);

		inline BufferHandle composeBufferHandle(uint16 resourceIndex) const;
		inline TextureHandle composeTextureHandle(uint16 resourceIndex) const;
		inline Resource& resolveBufferHandle(BufferHandle bufferHandle) const;
		inline Resource& resolveTextureHandle(TextureHandle textureHandle) const;

	public:
		TaskGraph() = default;
		inline ~TaskGraph() { destroy(); }

		void initialize();
		void destroy();

		void open(HAL::Device& hwDevice,
			HAL::CommandAllocatorHandle hwCommandAllocator,
			HAL::DescriptorAllocatorHandle hwTransientDescriptorAllocator,
			CircularUploadMemoryAllocator& transientUploadMemoryAllocator,
			TransientResourceCache& transientResourceCache);

		BufferHandle createTransientBuffer(uint32 size, uint64 nameXSH);
		TextureHandle createTransientTexture(HAL::TextureDesc hwTextureDesc, uint64 nameXSH);
		BufferHandle importExternalBuffer(HAL::BufferHandle hwBufferHandle);
		TextureHandle importExternalTexture(HAL::TextureHandle hwTextureHandle,
			HAL::TextureLayout hwPreExecutionLayout, HAL::TextureLayout hwPostExecutionLayout);

		void* allocateUserData(uint32 size);
		HAL::DescriptorSet allocateTransientDescriptorSet(HAL::DescriptorSetLayoutHandle hwDescriptorSetLayout);
		UploadBufferPointer allocateTransientUploadMemory(uint32 size);

		// void writeDescriptorDeferred();

		TaskDependencyCollector addTask(TaskType type, TaskExecutorFunc executorFunc, void* userData);

		void execute();
	};


	class TaskExecutionContext final : public XLib::NonCopyable
	{
		friend TaskGraph;

	private:
		HAL::Device& hwDevice;
		const TaskGraph& taskGraph;
		HAL::DescriptorAllocatorHandle hwTransientDescriptorAllocator;
		CircularUploadMemoryAllocator& transientUploadMemoryAllocator;

	private:
		TaskExecutionContext(HAL::Device& hwDevice, const TaskGraph& taskGraph,
			HAL::DescriptorAllocatorHandle hwTransientDescriptorAllocator,
			CircularUploadMemoryAllocator& transientUploadMemoryAllocator);
		~TaskExecutionContext() = default;

	public:
		HAL::DescriptorSet allocateTransientDescriptorSet(HAL::DescriptorSetLayoutHandle hwDescriptorSetLayout);
		UploadBufferPointer allocateTransientUploadMemory(uint32 size);

		HAL::BufferHandle resolveBuffer(BufferHandle hwBufferHandle);
		HAL::TextureHandle resolveTexture(TextureHandle hwTextureHandle);
	};
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// INLINE DEFINITIONS //////////////////////////////////////////////////////////////////////////////

namespace XEngine::Gfx::Scheduler
{
	inline HAL::BarrierSync TaskDependencyCollector::TranslateSchResourceShaderAccessStageToHwBarrierSync(ResourceShaderAccessStage shaderStage)
	{
		switch (shaderStage)
		{
			case ResourceShaderAccessStage::Any:		return HAL::BarrierSync::AllShaders;
			case ResourceShaderAccessStage::Compute:	return HAL::BarrierSync::ComputeShader;
			case ResourceShaderAccessStage::Graphics:	return HAL::BarrierSync::AllGraphicsShaders;
			case ResourceShaderAccessStage::PrePixel:	return HAL::BarrierSync::PrePixelShaders;
			case ResourceShaderAccessStage::Pixel:		return HAL::BarrierSync::PixelShader;
		}
		XEMasterAssertUnreachableCode();
		return HAL::BarrierSync::None;
	}

	inline TaskDependencyCollector::TaskDependencyCollector(TaskGraph& parent)
	{
		parent.registerIssuedTaskDependenciesCollector(*this);
	}

	inline TaskDependencyCollector::~TaskDependencyCollector()
	{
		if (parent)
			parent->revokeIssuedTaskDependenciesCollector();
	}

	inline TaskDependencyCollector::TaskDependencyCollector(TaskDependencyCollector&& that)
	{
		if (that.parent)
			that.parent->relocateIssuedTaskDependenciesCollector(that, *this);
	}

	inline void TaskDependencyCollector::operator = (TaskDependencyCollector&& that)
	{
		if (that.parent)
			that.parent->relocateIssuedTaskDependenciesCollector(that, *this);
	}

	inline auto TaskDependencyCollector::addBufferAcces(
		BufferHandle hwBufferHandle, HAL::BarrierSync hwSync, HAL::BarrierAccess hwAccess) -> TaskDependencyCollector&
	{
		parent->addTaskDependency(*this, HAL::ResourceType::Buffer, uint32(hwBufferHandle), hwSync, hwAccess, HAL::TextureLayout::Undefined);
		return *this;
	}

	inline auto TaskDependencyCollector::addTextureAccess(
		TextureHandle hwTextureHandle, HAL::BarrierSync hwSync, HAL::BarrierAccess hwAccess,
		HAL::TextureLayout hwTextureLayout, const HAL::TextureSubresourceRange* hwSubresourceRange) -> TaskDependencyCollector&
	{
		XEAssert(!hwSubresourceRange);
		parent->addTaskDependency(*this, HAL::ResourceType::Texture, uint32(hwTextureHandle), hwSync, hwAccess, hwTextureLayout);
		return *this;
	}

	inline TaskDependencyCollector& TaskDependencyCollector::addTextureShaderRead(TextureHandle hwTextureHandle,
		ResourceShaderAccessStage shaderStage, const HAL::TextureSubresourceRange* hwSubresourceRange)
	{
		parent->addTaskDependency(*this, HAL::ResourceType::Texture, uint32(hwTextureHandle),
			TranslateSchResourceShaderAccessStageToHwBarrierSync(shaderStage),
			HAL::BarrierAccess::ShaderReadOnly, HAL::TextureLayout::ShaderReadOnly);
		return *this;
	}

	inline TaskDependencyCollector& TaskDependencyCollector::addColorRenderTarget(
		TextureHandle hwTextureHandle/*, uint8 mipLevel, uint16 arrayIndex*/)
	{
		parent->addTaskDependency(*this, HAL::ResourceType::Texture, uint32(hwTextureHandle),
			HAL::BarrierSync::ColorRenderTarget, HAL::BarrierAccess::ColorRenderTarget, HAL::TextureLayout::ColorRenderTarget);
		return *this;
	}

	inline TaskDependencyCollector& TaskDependencyCollector::addDepthStencilRenderTarget(
		TextureHandle hwTextureHandle/*, uint8 mipLevel = 0, uint16 arrayIndex = 0*/)
	{
		parent->addTaskDependency(*this, HAL::ResourceType::Texture, uint32(hwTextureHandle),
			HAL::BarrierSync::DepthStencilRenderTarget, HAL::BarrierAccess::DepthStencilRenderTarget, HAL::TextureLayout::DepthStencilRenderTarget);
		return *this;
	}
}
