#include "XEngine.Gfx.Scheduler.h"

#include <XLib.Algorithm.QuickSort.h>
#include <XLib.Allocation.h>
#include <XLib.Containers.ArrayList.h>

#include "XEngine.Gfx.Allocation.h"

using namespace XEngine::Gfx;
using namespace XEngine::Gfx::Scheduler;


static inline HAL::BarrierSync TranslateSchResourceShaderAccessStageToHwBarrierSync(Scheduler::ResourceShaderAccessStage shaderStage)
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


// TransientResourceCache ///////////////////////////////////////////////////////////////////////////

struct TransientResourceCache::Entry
{
	uint64 nameXSH;
	HAL::DeviceQueueSyncPoint hwLastUsageCycleEOPSyncPoint;

	union
	{
		uint32 bufferSize;
		HAL::TextureDesc hwTextureDesc;
	} desc;

	union
	{
		HAL::BufferHandle hwBufferHandle;
		HAL::TextureHandle hwTextureHandle;
	};

	uint16 memoryOffset;

	HAL::ResourceType type;
};


void TransientResourceCache::openUsageCycle()
{

}

void TransientResourceCache::closeUsageCycle()
{

}

void TransientResourceCache::setPrevUsageCycleEOPSyncPoint(HAL::DeviceQueueSyncPoint hwSyncPoint)
{

}

HAL::BufferHandle TransientResourceCache::queryBuffer(uint64 nameXSH, uint32 bufferSize, uint16 memoryOffset)
{
	bufferSize = alignUp<uint32>(bufferSize, AllocationAlignment);

	for (uint16 i = 0; i < entryCount; i++)
	{
		const Entry& entry = entries[i];
		if (entry.type == HAL::ResourceType::Buffer &&
			entry.nameXSH == nameXSH &&
			entry.desc.bufferSize == bufferSize &&
			entry.memoryOffset == memoryOffset)
		{
			return entry.hwBufferHandle;
		}
	}

	const HAL::BufferHandle hwBuffer = hwDevice->createBuffer(bufferSize, hwDeviceMemory, memoryOffset);

	XEMasterAssert(entryCount < entryPoolSize);
	const uint16 newEntryIndex = entryCount;
	entryCount++;

	Entry& entry = entries[newEntryIndex];
	entry = {};
	entry.nameXSH = nameXSH;
	entry.hwLastUsageCycleEOPSyncPoint = {};
	entry.desc.bufferSize = bufferSize;
	entry.hwBufferHandle = hwBuffer;
	entry.memoryOffset = memoryOffset;
	entry.type = HAL::ResourceType::Buffer;
}

HAL::TextureHandle TransientResourceCache::queryTexture(uint64 nameXSH, HAL::TextureDesc hwTextureDesc, uint16 memoryOffset)
{

}

TransientResourceCache::~TransientResourceCache()
{
	if (hwDevice)
	{
		hwDevice->releaseDeviceMemory(hwDeviceMemory);
		hwDevice = nullptr;
		hwDeviceMemory = {};
		deviceMemorySize = 0;
	}

	if (entries)
	{
		XLib::SystemHeapAllocator::Release(entries);
		entries = nullptr;
		entryPoolSize = 0;
		entryCount = 0;
	}
}

void TransientResourceCache::initialize(HAL::Device& hwDevice, uint16 maxEntryCount)
{
	this->hwDevice = &hwDevice;
	deviceMemorySize = 1024 * 1024 * 128;
	hwDeviceMemory = hwDevice.allocateDeviceMemory(deviceMemorySize);
}


// TaskExecutionContext ////////////////////////////////////////////////////////////////////////////

TaskExecutionContext::TaskExecutionContext(HAL::Device& hwDevice, const TaskGraph& graph,
	HAL::DescriptorAllocatorHandle hwTransientDescriptorAllocator,
	CircularUploadMemoryAllocator& transientUploadMemoryAllocator)
	:
	hwDevice(hwDevice), taskGraph(taskGraph),
	hwTransientDescriptorAllocator(hwTransientDescriptorAllocator),
	transientUploadMemoryAllocator(transientUploadMemoryAllocator)
{ }

HAL::DescriptorSet TaskExecutionContext::allocateTransientDescriptorSet(HAL::DescriptorSetLayoutHandle descriptorSetLayout)
{
	return hwDevice.allocateDescriptorSet(hwTransientDescriptorAllocator, descriptorSetLayout);
}

UploadBufferPointer TaskExecutionContext::allocateTransientUploadMemory(uint32 size)
{
	return transientUploadMemoryAllocator.allocate(size);
}

HAL::BufferHandle TaskExecutionContext::resolveBuffer(BufferHandle bufferHandle)
{
	const uint16 resourceIndex = uint16(bufferHandle);
	XEAssert(resourceIndex < taskGraph.resourceCount);
	const TaskGraph::Resource& resource = taskGraph.resources[resourceIndex];
	XEAssert(resource.type == HAL::ResourceType::Buffer);
	return resource.hwBufferHandle;
}

HAL::TextureHandle TaskExecutionContext::resolveTexture(TextureHandle textureHandle)
{
	const uint16 resourceIndex = uint16(textureHandle);
	XEAssert(resourceIndex < taskGraph.resourceCount);
	const TaskGraph::Resource& resource = taskGraph.resources[resourceIndex];
	XEAssert(resource.type == HAL::ResourceType::Texture);
	return resource.hwTextureHandle;
}


// TaskGraph ///////////////////////////////////////////////////////////////////////////////////////////

enum class TaskGraph::State : uint8
{
	Undefined = 0,
	Empty,
	Recording,
	ReadyForExecution,
};

struct TaskGraph::Resource
{
	uint64 transientNameXSH;

	union
	{
		uint32 bufferSize;
		HAL::TextureDesc hwTextureDesc;
	} desc;

	union
	{
		HAL::BufferHandle hwBufferHandle;
		HAL::TextureHandle hwTextureHandle;
	};

	HAL::ResourceType type;
	bool isImported;
	//uint8 handleGeneration;

	HAL::TextureLayout hwPreExecutionImportedTextureLayout;
	HAL::TextureLayout hwPostExecutionImportedTextureLayout;

	uint16 dependencyChainHeadIdx;
	uint16 dependencyChainTailIdx;

	//uint16 firstUsagePassIndex;
	//uint16 lastUsagePassIndex;
};

struct TaskGraph::Task
{
	TaskExecutorFunc executporFunc;
	void* userData;

	uint16 dependenciesOffset;
	uint16 dependencyCount;

	struct
	{
		HAL::BarrierSync hwSyncBefore;
		HAL::BarrierSync hwSyncAfter;
		HAL::BarrierAccess hwAccessBefore;
		HAL::BarrierAccess hwAccessAfter;
	} preExecutionGlobalBarrier;

	uint16 preExecutionLocalBarrierChainHeadIdx;
};

struct TaskGraph::Dependency
{
	uint16 resourceIndex;
	uint16 taskIndex;

	HAL::BarrierSync hwSync;
	HAL::BarrierAccess hwAccess;
	HAL::TextureLayout hwTextureLayout;
	bool canOverlapPrecedingShaderWrite;

	HAL::TextureSubresourceRange hwTextureSubresourceRange;
	bool usesTextureSubresourceRange;

	uint16 resourceDependencyChainNextIdx;
};

struct TaskGraph::Barrier
{
	HAL::BarrierSync hwSyncBefore;
	HAL::BarrierSync hwSyncAfter;
	HAL::BarrierAccess hwAccessBefore;
	HAL::BarrierAccess hwAccessAfter;

	HAL::TextureSubresourceRange hwTextureSubresourceRange;
	HAL::TextureLayout hwTextureLayoutBefore;
	HAL::TextureLayout hwTextureLayoutAfter;

	uint16 resourceIndex;

	uint16 chainNextIdx;
};


void TaskGraph::registerIssuedTaskDependenciesCollector(TaskDependencyCollector& registree)
{
	XEAssert(!issuedTaskDependencyCollector);
	issuedTaskDependencyCollector = &registree;
	issuedTaskDependencyCollector->parent = this;
}

void TaskGraph::relocateIssuedTaskDependenciesCollector(TaskDependencyCollector& from, TaskDependencyCollector& to)
{
	XEAssert(issuedTaskDependencyCollector == &from);
	XEAssert(issuedTaskDependencyCollector->parent == this);
	issuedTaskDependencyCollector->parent = nullptr;
	issuedTaskDependencyCollector = &to;
	issuedTaskDependencyCollector->parent = this;
}

void TaskGraph::revokeIssuedTaskDependenciesCollector()
{
	if (!issuedTaskDependencyCollector)
		return;

	XEAssert(issuedTaskDependencyCollector->parent == this);
	issuedTaskDependencyCollector->parent = nullptr;
	issuedTaskDependencyCollector = nullptr;
}

void TaskGraph::addTaskDependency(TaskDependencyCollector& sourceTaskDependenlyCollector,
	HAL::ResourceType hwResourceType, uint32 hwResourceHandle, HAL::BarrierSync hwSync, HAL::BarrierAccess hwAccess)
{
	XEAssert(issuedTaskDependencyCollector);
	XEAssert(&sourceTaskDependenlyCollector == issuedTaskDependencyCollector);

	// TODO: Do actual validation for sync/access values.
	XEAssert(hwSync != HAL::BarrierSync::None);
	XEAssert(hwAccess != HAL::BarrierAccess::None);

	const uint16 resourceIndex = uint16(hwResourceHandle);
	Resource& resource = resources[resourceIndex];
	XEAssert(resource.type == hwResourceType);

	XEAssert(taskCount > 0);
	const uint16 taskIndex = taskCount - 1;
	Task& task = tasks[taskIndex];
	XEAssert(task.dependenciesOffset + task.dependencyCount == dependencyCount);

	const uint16 dependencyIndex = dependencyCount;
	XEMasterAssert(dependencyIndex < dependencyPoolSize);
	dependencyCount++;
	task.dependencyCount++;

	Dependency& dependency = dependencies[dependencyIndex];
	dependency = {};
	dependency.resourceIndex = resourceIndex;
	dependency.taskIndex = taskIndex;
	dependency.hwSync = hwSync;
	dependency.hwAccess = hwAccess;
	dependency.canOverlapPrecedingShaderWrite = false; // TODO: Implement proper support for this.
	dependency.resourceDependencyChainNextIdx = uint16(-1);

	if (resource.dependencyChainHeadIdx == uint16(-1))
		resource.dependencyChainHeadIdx = dependencyIndex;
	else
		dependencies[resource.dependencyChainTailIdx].resourceDependencyChainNextIdx = dependencyIndex;

	resource.dependencyChainTailIdx = dependencyIndex;
}

void TaskGraph::initialize()
{
	XEMasterAssert(!this->internalMemoryBlock);
	memorySet(this, 0, sizeof(TaskGraph));


	this->resourcePoolSize = MaxResourceCount;
	this->taskPoolSize = MaxTaskCount;
	this->dependencyPoolSize = MaxDenendencyCount;
	this->barrierPoolSize = MaxDenendencyCount * 2;
	this->userDataPoolSize = 16 * 1024; // TODO: ????

	{
		uintptr poolsMemorySizeAccum = 0;

		const uintptr resourcePoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Resource) * resourcePoolSize;

		const uintptr passPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Task) * taskPoolSize;

		const uintptr dependencyPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Dependency) * dependencyPoolSize;

		const uintptr barrierPoolMemoryOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Barrier) * barrierPoolSize;

		const uintptr userDataPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += userDataPoolSize;

		const uintptr poolsTotalMemorySize = poolsMemorySizeAccum;
		byte* poolsMemory = (byte*)XLib::SystemHeapAllocator::Allocate(poolsTotalMemorySize);
		memorySet(poolsMemory, 0, poolsTotalMemorySize);

		resources = (Resource*)(poolsMemory + resourcePoolMemOffset);
		tasks = (Task*)(poolsMemory + passPoolMemOffset);
		dependencies = (Dependency*)(poolsMemory + dependencyPoolMemOffset);
		barriers = (Barrier*)(poolsMemory + barrierPoolMemoryOffset);
		userDataPool = poolsMemory + userDataPoolMemOffset;
	}
}

void TaskGraph::destroy()
{
	if (internalMemoryBlock)
		XLib::SystemHeapAllocator::Release(internalMemoryBlock);

	memorySet(this, 0, sizeof(TaskGraph));
}

void TaskGraph::open(HAL::Device& hwDevice, TransientResourceCache& transientResourceCache)
{
	XEAssert(state == State::Empty);

	this->hwDevice = &hwDevice;
	this->transientResourceCache = &transientResourceCache;

	...;
	XEAssertUnreachableCode();

	postExecutionLocalBarrierChainHeadIdx = uint16(-1);
	state = State::Recording;
}

BufferHandle TaskGraph::createTransientBuffer(uint32 size, uint64 nameXSH)
{
	XEAssert(state == State::Recording);

	XEMasterAssert(resourceCount < resourcePoolSize);
	const uint16 resourceIndex = resourceCount;
	resourceCount++;

	Resource& resource = resources[resourceIndex];
	resource = {};
	resource.bufferSize = size;
	resource.transientResourceNameXSH = nameXSH;
	resource.type = HAL::ResourceType::Buffer;
	resource.isImported = false;
	resource.dependencyChainHeadIdx = uint16(-1);
	resource.dependencyChainTailIdx = uint16(-1);

	return BufferHandle(resourceIndex);
}

TextureHandle TaskGraph::createTransientTexture(HAL::TextureDesc hwTextureDesc, uint64 nameXSH)
{
	XEAssert(state == State::Recording);

	XEMasterAssert(resourceCount < resourcePoolSize);
	const uint16 resourceIndex = resourceCount;
	resourceCount++;

	Resource& resource = resources[resourceIndex];
	resource = {};
	resource.hwTextureDesc = hwTextureDesc;
	resource.transientResourceNameXSH = nameXSH;
	resource.type = HAL::ResourceType::Texture;
	resource.isImported = false;
	resource.dependencyChainHeadIdx = uint16(-1);
	resource.dependencyChainTailIdx = uint16(-1);

	return TextureHandle(resourceIndex);
}

BufferHandle TaskGraph::importExternalBuffer(HAL::BufferHandle hwBufferHandle)
{
	XEAssert(state == State::Recording);

	XEMasterAssert(resourceCount < resourcePoolSize);
	const uint16 resourceIndex = resourceCount;
	resourceCount++;

	Resource& resource = resources[resourceIndex];
	resource = {};
	// TODO: Fill `resource.bufferSize` just in case.
	resource.hwBufferHandle = hwBufferHandle;
	resource.type = HAL::ResourceType::Buffer;
	resource.isImported = true;
	resource.dependencyChainHeadIdx = uint16(-1);
	resource.dependencyChainTailIdx = uint16(-1);

	return BufferHandle(resourceIndex);
}

TextureHandle TaskGraph::importExternalTexture(HAL::TextureHandle hwTextureHandle,
	HAL::TextureLayout hwPreExecutionLayout, HAL::TextureLayout hwPostExecutionLayout)
{
	XEAssert(state == State::Recording);

	XEMasterAssert(resourceCount < resourcePoolSize);
	const uint16 resourceIndex = resourceCount;
	resourceCount++;

	Resource& resource = resources[resourceIndex];
	resource = {};
	// TODO: Fill `resource.textureDesc` just in case.
	resource.hwTextureHandle = hwTextureHandle;
	resource.type = HAL::ResourceType::Texture;
	resource.isImported = true;
	resource.hwPreExecutionImportedTextureLayout = hwPreExecutionLayout;
	resource.hwPostExecutionImportedTextureLayout = hwPostExecutionLayout;
	resource.dependencyChainHeadIdx = uint16(-1);
	resource.dependencyChainTailIdx = uint16(-1);

	return TextureHandle(resourceIndex);
}

TaskDependencyCollector TaskGraph::addTask(TaskType type, TaskExecutorFunc executorFunc, void* userData)
{
	XEAssert(state == State::Recording);
	revokeIssuedTaskDependenciesCollector();

	XEMasterAssert(taskCount < taskPoolSize);
	const uint16 taskIndex = taskCount;
	taskCount++;

	Task& task = tasks[taskIndex];
	task.executporFunc = executorFunc;
	task.userData = userData;
	task.dependenciesOffset = dependencyCount;
	task.dependencyCount = 0;
	task.preExecutionGlobalBarrier = {};
	task.preExecutionLocalBarrierChainHeadIdx = uint16(-1);

	return TaskDependencyCollector(*this);
}

void TaskGraph::closeAndCompile()
{
	XEAssert(state == State::Recording);
	revokeIssuedTaskDependenciesCollector();

	// Generate resource barriers.
	{
		struct ResourceState
		{
			union
			{
				struct
				{
					uint16 prevGlobalBarrierEmittingTaskIndex;
				} buffer;

				struct
				{
					uint16 prevBarrierIndex;
				} texture;
			};

			HAL::BarrierSync hwSync;
			HAL::BarrierAccess hwAccess;
			HAL::TextureLayout hwTextureLayout;
		};

		ResourceState resourceStates[MaxResourceCount];
		XEAssert(resourceCount < MaxResourceCount);
		memorySet(resourceStates, 0, sizeof(ResourceState) * resourceCount);

		for (uint16 taskIndex = 0; taskIndex < taskCount; taskIndex++)
		{
			Task& task = tasks[taskIndex];
			for (uint16 taskDependencyIndex = 0; taskDependencyIndex < task.dependencyCount; taskDependencyIndex++)
			{
				const Dependency& dependency = dependencies[task.dependenciesOffset + taskDependencyIndex];

				const uint16 resourceIndex = dependency.resourceIndex;
				XEAssert(resourceIndex < resourceCount);
				const Resource& resource = resources[resourceIndex];
				ResourceState& resourceState = resourceStates[resourceIndex];

				if (resource.type == HAL::ResourceType::Buffer)
				{
					XEAssert(HAL::BarrierSyncUtils::IsBufferCompatible(dependency.hwSync));
					XEAssert(HAL::BarrierAccessUtils::IsBufferCompatible(dependency.hwAccess));

					if (resourceState.hwAccess == HAL::BarrierAccess::None)
					{
						XEAssert(resourceState.hwSync == HAL::BarrierSync::None);

						// This is first access to buffer. No barrier required.

						resourceState.buffer.prevGlobalBarrierEmittingTaskIndex = uint16(-1);
						resourceState.hwSync = dependency.hwSync;
						resourceState.hwAccess = dependency.hwAccess;
					}
					else
					{
						XEAssert(resourceState.hwSync != HAL::BarrierSync::None);

						// Buffer was already accessed.
						// Extend previously generated barrier if this is read->read situation. Generate new barrier otherwise.

						const bool prevAccessIsReadOnly = HAL::BarrierAccessUtils::IsReadOnly(resourceState.hwAccess);
						const bool currAccessIsReadOnly = HAL::BarrierAccessUtils::IsReadOnly(dependency.hwAccess);
						if (prevAccessIsReadOnly && currAccessIsReadOnly)
						{
							// This is read->read situation. No new barrier required. Just extend previous access.

							const uint16 prevGlobalBarrierEmittingTaskIndex = resourceState.buffer.prevGlobalBarrierEmittingTaskIndex;
							if (prevGlobalBarrierEmittingTaskIndex == uint16(-1))
							{
								// We are extending first access to resource that does not require barrier.
								// Nothing to do here.
							}
							else
							{
								// Extend previous barrer.
								XEAssert(prevGlobalBarrierEmittingTaskIndex < taskCount);
								Task& prevGlobalBarrierEmittingTask = tasks[prevGlobalBarrierEmittingTaskIndex];
								prevGlobalBarrierEmittingTask.preExecutionGlobalBarrier.hwSyncAfter |= dependency.hwSync;
								prevGlobalBarrierEmittingTask.preExecutionGlobalBarrier.hwAccessAfter |= dependency.hwAccess;
							}

							resourceState.hwSync |= dependency.hwSync;
							resourceState.hwAccess |= dependency.hwAccess;
						}
						else
						{
							// Generate new barrier.

							// TODO: Handle `canOverlapPrecedingShaderWrite`.

							task.preExecutionGlobalBarrier.hwSyncBefore |= resourceState.hwSync;
							task.preExecutionGlobalBarrier.hwSyncAfter |= dependency.hwSync;
							task.preExecutionGlobalBarrier.hwAccessBefore |= resourceState.hwAccess;
							task.preExecutionGlobalBarrier.hwAccessAfter |= dependency.hwAccess;

							resourceState.buffer.prevGlobalBarrierEmittingTaskIndex = taskIndex;
							resourceState.hwSync = dependency.hwSync;
							resourceState.hwAccess = dependency.hwAccess;
						}
					}
				}
				else if (resource.type == HAL::ResourceType::Texture)
				{
					XEAssert(HAL::BarrierSyncUtils::IsTextureCompatible(dependency.hwSync));
					XEAssert(HAL::BarrierAccessUtils::IsCompatibleWithTextureLayout(dependency.hwAccess, dependency.hwTextureLayout));
					XEAssert(dependency.hwTextureLayout != HAL::TextureLayout::Undefined);

					XEAssert(!dependency.usesTextureSubresourceRange); // Not implemented.

					if (resourceState.hwAccess == HAL::BarrierAccess::None)
					{
						XEAssert(resourceState.hwSync == HAL::BarrierSync::None);
						XEAssert(resourceState.hwTextureLayout == HAL::TextureLayout::Undefined);

						// This is first access to texture.
						// For imported texture: generate layout transition barrier if pre-execution layout does not match dependency layout.
						// For transient texture: communicate initial texture layout to driver via "fake" barrier.

						bool shouldGenerateBarrier = false;

						if (resource.isImported)
						{
							if (resource.hwPreExecutionImportedTextureLayout != dependency.hwTextureLayout)
							{
								// Real layout transition.
								shouldGenerateBarrier = true;
								resourceState.hwTextureLayout = resource.hwPreExecutionImportedTextureLayout;
							}
							else
							{
								// Access does not require barrier.
								shouldGenerateBarrier = false;
							}
						}
						else
						{
							// Fake transition to notify driver about initial layout.
							shouldGenerateBarrier = true;
						}

						uint16 generatedBarrierIndex = uint16(-1);
						if (shouldGenerateBarrier)
						{
							XEMasterAssert(barrierCount < barrierPoolSize);
							const uint16 barrierIndex = barrierCount;
							barrierCount++;

							Barrier& barrier = barriers[barrierIndex];
							barrier = {};
							barrier.hwSyncBefore = HAL::BarrierSync::None;
							barrier.hwAccessBefore = HAL::BarrierAccess::None;
							barrier.hwSyncAfter = dependency.hwSync;
							barrier.hwAccessAfter = dependency.hwAccess;
							//barrier.hwTextureSubresourceRange = ...;
							barrier.hwTextureLayoutBefore = resourceState.hwTextureLayout;
							barrier.hwTextureLayoutAfter = dependency.hwTextureLayout;
							barrier.resourceIndex = resourceIndex;
							barrier.chainNextIdx = task.preExecutionLocalBarrierChainHeadIdx;
							task.preExecutionLocalBarrierChainHeadIdx = barrierIndex;

							generatedBarrierIndex = barrierIndex;
						}

						resourceState.texture.prevBarrierIndex = generatedBarrierIndex;
						resourceState.hwSync = dependency.hwSync;
						resourceState.hwAccess = dependency.hwAccess;
						resourceState.hwTextureLayout = dependency.hwTextureLayout;
					}
					else
					{
						XEAssert(resourceState.hwSync != HAL::BarrierSync::None);
						XEAssert(resourceState.hwTextureLayout != HAL::TextureLayout::Undefined);

						// Texture was already accessed.
						// Extend previous barrier if layout matches. Generate new barrier otherwise.

						if (resourceState.hwTextureLayout == dependency.hwTextureLayout)
						{
							const bool prevAccessIsReadOnly = HAL::BarrierAccessUtils::IsReadOnly(resourceState.hwAccess);
							const bool currAccessIsReadOnly = HAL::BarrierAccessUtils::IsReadOnly(dependency.hwAccess);

							if (prevAccessIsReadOnly && currAccessIsReadOnly)
							{
								// This is read->read situation. No new barrier required. Just extend previous access.

								const uint16 prevBarrierIndex = resourceState.texture.prevBarrierIndex;
								if (prevBarrierIndex == uint16(-1))
								{
									// We are extending first access to resource that does not require barrier.
									// Nothing to do here.
								}
								else
								{
									// Extend previous barrer.
									XEAssert(prevBarrierIndex < barrierCount);
									Barrier& prevBarrier = barriers[prevBarrierIndex];
									prevBarrier.hwSyncAfter |= dependency.hwSync;
									prevBarrier.hwAccessAfter |= dependency.hwAccess;
								}

								resourceState.hwSync |= dependency.hwSync;
								resourceState.hwAccess |= dependency.hwAccess;
							}
							else
							{
								// Generate new barrier.

								// TODO: Handle `canOverlapPrecedingShaderWrite`.

								XEMasterAssert(barrierCount < barrierPoolSize);
								const uint16 barrierIndex = barrierCount;
								barrierCount++;

								Barrier& barrier = barriers[barrierIndex];
								barrier = {};
								barrier.hwSyncBefore = resourceState.hwSync;
								barrier.hwAccessBefore = resourceState.hwAccess;
								barrier.hwSyncAfter = dependency.hwSync;
								barrier.hwAccessAfter = dependency.hwAccess;
								//barrier.hwTextureSubresourceRange = ...;
								barrier.hwTextureLayoutBefore = resourceState.hwTextureLayout;
								barrier.hwTextureLayoutAfter = resourceState.hwTextureLayout;
								barrier.resourceIndex = resourceIndex;
								barrier.chainNextIdx = task.preExecutionLocalBarrierChainHeadIdx;
								task.preExecutionLocalBarrierChainHeadIdx = barrierIndex;

								resourceState.texture.prevBarrierIndex = barrierIndex;
								resourceState.hwSync = dependency.hwSync;
								resourceState.hwAccess = dependency.hwAccess;
							}
						}
						else
						{
							XEMasterAssert(barrierCount < barrierPoolSize);
							const uint16 barrierIndex = barrierCount;
							barrierCount++;

							Barrier& barrier = barriers[barrierIndex];
							barrier = {};
							barrier.hwSyncBefore = resourceState.hwSync;
							barrier.hwAccessBefore = resourceState.hwAccess;
							barrier.hwSyncAfter = dependency.hwSync;
							barrier.hwAccessAfter = dependency.hwAccess;
							//barrier.hwTextureSubresourceRange = ...;
							barrier.hwTextureLayoutBefore = resourceState.hwTextureLayout;
							barrier.hwTextureLayoutAfter = dependency.hwTextureLayout;
							barrier.resourceIndex = resourceIndex;
							barrier.chainNextIdx = task.preExecutionLocalBarrierChainHeadIdx;
							task.preExecutionLocalBarrierChainHeadIdx = barrierIndex;

							resourceState.texture.prevBarrierIndex = barrierIndex;
							resourceState.hwSync = dependency.hwSync;
							resourceState.hwAccess = dependency.hwAccess;
							resourceState.hwTextureLayout = dependency.hwTextureLayout;
						}
					}
				}
				else
					XEAssertUnreachableCode();
			}
		}


		// Generate barriers to transition imported textures to post-execution layout.
		XEAssert(postExecutionLocalBarrierChainHeadIdx == uint16(-1));
		for (uint16 resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
		{
			const Resource& resource = resources[resourceIndex];
			const ResourceState& resourceState = resourceStates[resourceIndex];

			if (resource.isImported &&
				resource.type == HAL::ResourceType::Texture &&
				resource.hwPostExecutionImportedTextureLayout != resourceState.hwTextureLayout)
			{
				XEMasterAssert(barrierCount < barrierPoolSize);
				const uint16 barrierIndex = barrierCount;
				barrierCount++;

				Barrier& barrier = barriers[barrierIndex];
				barrier = {};
				barrier.hwSyncBefore = resourceState.hwSync;
				barrier.hwAccessBefore = resourceState.hwAccess;
				barrier.hwSyncAfter = HAL::BarrierSync::None;
				barrier.hwAccessAfter = HAL::BarrierAccess::None;
				//barrier.hwTextureSubresourceRange = ...;
				barrier.hwTextureLayoutBefore = resourceState.hwTextureLayout;
				barrier.hwTextureLayoutAfter = resource.hwPostExecutionImportedTextureLayout;
				barrier.resourceIndex = resourceIndex;
				barrier.chainNextIdx = postExecutionLocalBarrierChainHeadIdx;
				postExecutionLocalBarrierChainHeadIdx = barrierIndex;
			}
		}
	}


	// Compute transient resources locations.

	struct TransientResourceLocation
	{
		uint16 offset;
		uint16 size;
	};

	// NOTE: We need to store separate size here, as it can be different from actual resource size.
	// This can happen when we inherit resource location from previous run, but resource shrinks in size.
	// We still consider entire memory range of inherited resource as occupied when placing new resources.
	// To properly "shrink" resource we need 
	// ^^^ This is bullshit.

	TransientResourceLocation transientResourceLocations[MaxResourceCount];

#if 0

	{
		// [REMOVE_THIS_STUPID COMMENT] Transient resources are ordered from large to small.
		//struct SizeOrderedResourceListItem
		//{
		//	uint16 size;
		//	uint16 resourceIndex;
		//};

		XLib::InplaceArrayList<uint32, MaxResourceCount> sizeOrderedResourceList;

		for (uint16 i = 0; i < resourceCount; i++)
		{
			const Resource resource = resources[i];
			if (resource.isImported)
				continue;

			sizeOrderedResourceList.pushBack();
		}

		XLib::QuickSort(sizeOrderedResourceList.getData(), sizeOrderedResourceList.getSize());

		// ConflictingResourceBoundary value encoding:
		//	bit 0: boundary begin/end flag (begin - 1, end - 0)
		//	bits 1..15: boundary offset
		XLib::InplaceArrayList<uint16, MaxResourceCount * 2> conflictingResourceBoundaries;

		// Iterate through all transient resources from largest to smallest
		// Place 
		for (uint16 sizeOrderedResourceIdx = 0; sizeOrderedResourceIdx < sizeOrderedResourceList.getSize(); sizeOrderedResourceIdx++)
		{
			const uint32 sizeOrderedResourceListItem = sizeOrderedResourceList[sizeOrderedResourceIdx];
			const uint16 resourceIndex = ... sizeOrderedResourceListItem;
			const uint16 resourceSize = ... sizeOrderedResourceListItem;

			const Resource& resource = resources[resourceIndex];

			// Iterate over already placed resources.
			// Generate conflicting resource boundaries list.
			conflictingResourceBoundaries.clear();
			for (uint16 i = 0; i < sizeOrderedResourceIdx; i++)
			{
				const uint16 alreadyPlacedResourceIndex = ...;
				const uint16 alreadyPlaced
					const Resource& alreadyPlacedResource = resources[alreadyPlacedResourceIndex];

				const bool lifetimesOverlap =
					resource.firstUsageTaskIndex <= alreadyPlacedResource.lastUsageTaskIndex &&
					resource.lastUsageTaskIndex  >= alreadyPlacedResource.firstUsageTaskIndex;

				if (lifetimesOverlap)
				{
					const TransientResourceLocation& location = transientResourceLocations[alreadyPlacedResourceIndex];
					const uint16 beginOffset = location.offset;
					const uint16 endOffset = location.offset + location.size;

					conflictingResourceBoundaries.pushBack((beginOffset << 1) | 1);
					conflictingResourceBoundaries.pushBack((endOffset   << 1) | 0);
				}
			}

			// Sort conflicting resource boundaries list by memory offset.
			XLib::QuickSort<uint16>(conflictingResourceBoundaries.getData(), conflictingResourceBoundaries.getSize());

			// Add fake begin boundary representing end of entire transient memory heap.
			{
				const uint16 transientResourceHeapEndOffset = ...;
				conflictingResourceBoundaries.pushBack((transientResourceHeapEndOffset << 1) | 1);
			}

			// Iterate over conflicting resource boundaries.
			// Look for free ranges. Find smallest free range that fits the resource.
			uint16 conflictCount = 0;
			uint16 suitableRangeSize = uint16(-1);
			uint16 suitableRangeOffset = 0;
			uint16 currentRangeOffset = 0;
			for (uint16 conflictingResourceBoundary : conflictingResourceBoundaries)
			{
				const uint16 boundaryOffset = conflictingResourceBoundary >> 1;
				const uint16 isBeginBoundary = conflictingResourceBoundary & 1;

				if (conflictCount == 0)
				{
					// This is free range.
					XAssert(boundaryOffset > currentRangeOffset);
					const uint16 rangeSize = boundaryOffset - currentRangeOffset;

					if (rangeSize >= resourceSize &&
						suitableRangeSize > rangeSize)
					{
						suitableRangeSize = rangeSize;
						suitableRangeOffset = currentRangeOffset;
					}
				}

				conflictCount += (isBeginBoundary << 1) - 1;
				XEAssert(conflictCount < 0x8000);
			}

			// Check if no suitable range found.
			XEMasterAssert(suitableRangeSize != uint16(-1));

			// Set resource location to found free range.
			TransientResourceLocation& resourceLocation = transientResourceLocations[resourceIndex];
			resourceLocation.offset = suitableRangeOffset;
			resourceLocation.size = resourceSize;
		}
	}

#else

	uint32 transientResourceMemoryOffsetAccum = 0;
	for (uint16 resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
	{
		const Resource& resource = resources[resourceIndex];
		if (resource.isImported)
			continue;

		TransientResourceLocation& resourceLocation = transientResourceLocations[resourceIndex];

		if (resource.type == HAL::ResourceType::Buffer)
		{
			resourceLocation.offset = transientResourceMemoryOffsetAccum;
			resourceLocation.size = XCheckedCastU16(
				divRoundUp<uint64>(resource.desc.bufferSize, TransientResourceCache::AllocationAlignment));
		}
		else if (resource.type == HAL::ResourceType::Texture)
		{
			const HAL::ResourceMemoryRequirements hwMemoryRequirements =
				hwDevice->getTextureMemoryRequirements(resource.desc.hwTextureDesc);
			XEAssert(hwMemoryRequirements.alignment <= HAL::ResourceAlignmentRequirement::_64kib);

			resourceLocation.offset = transientResourceMemoryOffsetAccum;
			resourceLocation.size = XCheckedCastU16(
				divRoundUp<uint64>(hwMemoryRequirements.size, TransientResourceCache::AllocationAlignment));
		}

		transientResourceMemoryOffsetAccum += resourceLocation.size;
	}

#endif


	// Query transient resources from cache.
	{
		transientResourceCache->openUsageCycle();

		for (uint16 resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
		{
			Resource& resource = resources[resourceIndex];
			if (resource.isImported)
				continue;

			const TransientResourceLocation& resourceLocation = transientResourceLocations[resourceIndex];

			if (resource.type == HAL::ResourceType::Buffer)
			{
				resource.hwBufferHandle = transientResourceCache->queryBuffer(
					resource.transientNameXSH, resource.desc.bufferSize, resourceLocation.offset);
			}
			else if (resource.type == HAL::ResourceType::Texture)
			{
				resource.hwTextureHandle = transientResourceCache->queryTexture(
					resource.transientNameXSH, resource.desc.hwTextureDesc, resourceLocation.offset);
			}
		}

		transientResourceCache->closeUsageCycle();
	}

	state = State::ReadyForExecution;
}

void TaskGraph::execute(HAL::CommandAllocatorHandle hwCommandAllocator,
	HAL::DescriptorAllocatorHandle hwTransientDescriptorAllocator,
	CircularUploadMemoryAllocator& transientUploadMemoryAllocator)
{
	XEAssert(state == State::ReadyForExecution);

	if (!taskCount)
		return;

	TaskExecutionContext context(*hwDevice, *this,
		hwTransientDescriptorAllocator, transientUploadMemoryAllocator);

	HAL::CommandList hwCommandList;
	hwDevice->openCommandList(hwCommandList, hwCommandAllocator);

	for (uint16 taskIndex = 0; taskIndex < taskCount; taskIndex++)
	{
		const Task& task = tasks[taskIndex];

		// Emit barriers.
		{
			if (task.preExecutionGlobalBarrier.hwSyncBefore != HAL::BarrierSync::None &&
				task.preExecutionGlobalBarrier.hwSyncAfter != HAL::BarrierSync::None)
			{
				hwCommandList.globalMemoryBarrier(
					task.preExecutionGlobalBarrier.hwSyncBefore, task.preExecutionGlobalBarrier.hwSyncAfter,
					task.preExecutionGlobalBarrier.hwAccessBefore, task.preExecutionGlobalBarrier.hwAccessAfter);
			}

			uint16 barrierChainIt = task.preExecutionLocalBarrierChainHeadIdx;
			while (barrierChainIt != uint16(-1))
			{
				XAssert(barrierChainIt < barrierCount);
				const Barrier& barrier = barriers[barrierChainIt];
				XAssert(barrier.resourceIndex < resourceCount);
				const Resource& resource = resources[barrier.resourceIndex];
				XEAssert(resource.type == HAL::ResourceType::Texture);

				hwCommandList.textureMemoryBarrier(resource.hwTextureHandle,
					barrier.hwSyncBefore, barrier.hwSyncAfter,
					barrier.hwAccessBefore, barrier.hwAccessAfter,
					barrier.hwTextureLayoutBefore, barrier.hwTextureLayoutAfter);

				barrierChainIt = barrier.chainNextIdx;
			}
		}

		task.executporFunc(context, *hwDevice, hwCommandList, task.userData);
	}

	hwDevice->closeCommandList(hwCommandList);
	hwDevice->submitCommandList(HAL::DeviceQueue::Graphics, hwCommandList);

	const HAL::DeviceQueueSyncPoint hwEOPSyncPoint = hwDevice->getEOPSyncPoint(HAL::DeviceQueue::Graphics);
	transientUploadMemoryAllocator.enqueueRelease(hwEOPSyncPoint);
	transientResourceCache->setPrevUsageCycleEOPSyncPoint(hwEOPSyncPoint);
}
