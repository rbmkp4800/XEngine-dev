#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Gfx.HAL.D3D12.h>

#include "XEngine.Gfx.Allocation.h"

// TODO: Use hashmap or something in `TransientResourceCache` instead of retarded linear search through all entries.
// TODO: Optimal resource aliasing.
// TODO: Proper `TransientMemoryPool` implementation with multiple allocations and ability to grow.
// TODO: Check that there is one and only one dependency between task and resource(subresource).
// TODO: Check that we do not have duplacete imported resources during `TaskGraph::closeAndCompile` (as a separate validation step).
// TODO: Implement proper `canOverlapPrecedingShaderWrite` handling.
// TODO: Implement proper handles.

// TODO: Do something about this `union { textureDesc; bufferSize; };` everywhere. Probably we can drop buffer size and always save resource size.

namespace XEngine::Gfx::Scheduler
{
	class TaskGraph;
	class TaskExecutionContext;

	enum class BufferHandle : uint32 { Zero = 0, };
	enum class TextureHandle : uint32 { Zero = 0, };

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
		friend TaskGraph;

	private:
		static constexpr uint64 AllocationAlignment = 0x10000;

		struct Entry;

	private:
		HAL::Device* hwDevice = nullptr;
		HAL::DeviceMemoryHandle hwDeviceMemory = {};
		uint64 deviceMemorySize = 0;

		Entry* entries = nullptr;
		uint16 entryPoolSize = 0;
		uint16 entryCount = 0;

	private:
		void openUsageCycle();
		void closeUsageCycle();
		void setPrevUsageCycleEOPSyncPoint(HAL::DeviceQueueSyncPoint hwSyncPoint);

		HAL::BufferHandle queryBuffer(uint64 nameXSH, uint32 bufferSize, uint16 memoryOffset);
		HAL::TextureHandle queryTexture(uint64 nameXSH, HAL::TextureDesc hwTextureDesc, uint16 memoryOffset);

	public:
		TransientResourceCache() = default;
		~TransientResourceCache();

		void initialize(HAL::Device& hwDevice, uint16 maxEntryCount);
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


	class TaskDependencyCollector final : public XLib::NonCopyable
	{
		friend TaskGraph;

	private:
		TaskGraph* parent = nullptr;

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
			HAL::TextureSubresourceRange* hwSubresourceRange);

		//void addManualBufferAccess(BufferHandle hwBufferHandle);
		//void addManualTextureAccess(TextureHandle hwTextureHandle);

		inline TaskDependencyCollector& addBufferShaderRead(BufferHandle hwBufferHandle,
			ResourceShaderAccessStage shaderStage = ResourceShaderAccessStage::Any);
		inline TaskDependencyCollector& addBufferShaderWrite(BufferHandle hwBufferHandle,
			ResourceShaderAccessStage shaderStage = ResourceShaderAccessStage::Any,
			bool dependsOnPrecedingShaderWrite = true);

		inline TaskDependencyCollector& addTextureShaderRead(TextureHandle hwTextureHandle,
			const HAL::TextureSubresourceRange* hwSubresourceRange = nullptr,
			ResourceShaderAccessStage shaderStage = ResourceShaderAccessStage::Any);
		inline TaskDependencyCollector& addTextureShaderWrite(TextureHandle hwTextureHandle,
			const HAL::TextureSubresourceRange* hwSubresourceRange = nullptr,
			ResourceShaderAccessStage shaderStage = ResourceShaderAccessStage::Any,
			bool dependsOnPrecedingShaderWrite = true);

		inline TaskDependencyCollector& addColorRenderTarget(TextureHandle hwTextureHandle,
			uint8 mipLevel = 0, uint16 arrayIndex = 0);
		inline TaskDependencyCollector& addDepthStencilRenderTarget(TextureHandle hwTextureHandle,
			uint8 mipLevel = 0, uint16 arrayIndex = 0);
		inline TaskDependencyCollector& addDepthStencilRenderTargetReadOnly(TextureHandle hwTextureHandle,
			uint8 mipLevel = 0, uint16 arrayIndex = 0);
	};


	class TaskGraph final : public XLib::NonCopyable
	{
		friend TaskExecutionContext;
		friend TaskDependencyCollector;

	private:
		enum class State : uint8;

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
		byte* internalMemoryBlock = nullptr;
		Resource* resources = nullptr;
		Task* tasks = nullptr;
		Dependency* dependencies = nullptr;
		Barrier* barriers = nullptr;
		byte* userDataPool = nullptr;

		HAL::Device* hwDevice = nullptr;
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
		State state = State(0);

	private:
		void registerIssuedTaskDependenciesCollector(TaskDependencyCollector& registree);
		void relocateIssuedTaskDependenciesCollector(TaskDependencyCollector& from, TaskDependencyCollector& to);
		void revokeIssuedTaskDependenciesCollector();

		void addTaskDependency(TaskDependencyCollector& sourceTaskDependenlyCollector,
			HAL::ResourceType hwResourceType, uint32 hwResourceHandle, HAL::BarrierSync hwSync, HAL::BarrierAccess hwAccess);

	public:
		TaskGraph() = default;
		inline ~TaskGraph() { destroy(); }

		void initialize();
		void destroy();

		void open(HAL::Device& hwDevice, TransientResourceCache& transientResourceCache);

		BufferHandle createTransientBuffer(uint32 size, uint64 nameXSH);
		TextureHandle createTransientTexture(HAL::TextureDesc hwTextureDesc, uint64 nameXSH);
		BufferHandle importExternalBuffer(HAL::BufferHandle hwBufferHandle);
		TextureHandle importExternalTexture(HAL::TextureHandle hwTextureHandle,
			HAL::TextureLayout hwPreExecutionLayout, HAL::TextureLayout hwPostExecutionLayout);

		void* allocateUserData(uint32 size);

		TaskDependencyCollector addTask(TaskType type, TaskExecutorFunc executorFunc, void* userData);

		void closeAndCompile();

		void execute(HAL::CommandAllocatorHandle hwCommandAllocator,
			HAL::DescriptorAllocatorHandle hwTransientDescriptorAllocator,
			CircularUploadMemoryAllocator& transientUploadMemoryAllocator);
	};
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// INLINE DEFINITIONS //////////////////////////////////////////////////////////////////////////////

inline XEngine::Gfx::Scheduler::TaskDependencyCollector::TaskDependencyCollector(TaskGraph& parent)
{
	parent.registerIssuedTaskDependenciesCollector(*this);
}

inline XEngine::Gfx::Scheduler::TaskDependencyCollector::~TaskDependencyCollector()
{
	if (parent)
		parent->revokeIssuedTaskDependenciesCollector();
}

inline XEngine::Gfx::Scheduler::TaskDependencyCollector::TaskDependencyCollector(TaskDependencyCollector&& that)
{
	if (that.parent)
		that.parent->relocateIssuedTaskDependenciesCollector(that, *this);
}

inline void XEngine::Gfx::Scheduler::TaskDependencyCollector::operator = (TaskDependencyCollector&& that)
{
	if (that.parent)
		that.parent->relocateIssuedTaskDependenciesCollector(that, *this);
}

inline auto XEngine::Gfx::Scheduler::TaskDependencyCollector::addBufferAcces(
	BufferHandle hwBufferHandle, HAL::BarrierSync hwSync, HAL::BarrierAccess hwAccess) -> TaskDependencyCollector&
{
	parent->addTaskDependency(*this, HAL::ResourceType::Buffer, uint32(hwBufferHandle), hwSync, hwAccess);
	return *this;
}

inline auto XEngine::Gfx::Scheduler::addTextureAccess(TextureHandle hwTextureHandle,
	HAL::BarrierSync hwSync, HAL::BarrierAccess hwAccess, HAL::TextureLayout hwTextureLayout,
	HAL::TextureSubresourceRange* hwSubresourceRange)  -> TaskDependencyCollector&
{
	...;
	return *this;
}
