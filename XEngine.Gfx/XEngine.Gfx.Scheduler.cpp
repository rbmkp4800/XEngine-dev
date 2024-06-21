#include <XLib.Algorithm.QuickSort.h>
#include <XLib.Allocation.h>
#include <XLib.Containers.ArrayList.h>

#include "XEngine.Gfx.Scheduler.h"

using namespace XEngine::Gfx;
using namespace XEngine::Gfx::Scheduler;

XTODO("Implement proper handles");


static inline HAL::BarrierSync TranslateSchedulerShaderStageToHALBarrierSync(Scheduler::ShaderStage shaderStage)
{
	switch (shaderStage)
	{
		case ShaderStage::Any:		return HAL::BarrierSync::AllShaders;
		case ShaderStage::Compute:	return HAL::BarrierSync::ComputeShader;
		case ShaderStage::Graphics:	return HAL::BarrierSync::AllGraphicsShaders;
		case ShaderStage::PrePixel:	return HAL::BarrierSync::PrePixelShaders;
		case ShaderStage::Pixel:	return HAL::BarrierSync::PixelShader;
	}
	XEAssertUnreachableCode();
	return HAL::BarrierSync::None;
}


// TransientResourceCache ///////////////////////////////////////////////////////////////////////////

enum class TransientResourceCache::EntryType : uint8
{
	Undefined = 0,
	Buffer,
	Texture,
};

struct TransientResourceCache::Entry
{
	union
	{
		struct
		{
			HAL::BufferHandle handle;
			uint32 size;
		} buffer;

		struct
		{
			HAL::TextureHandle handle;
			HAL::TextureDesc desc;
		} texture;
	};

	EntryType type;
};

void TransientResourceCache::initialize(HAL::Device& device,
	HAL::DeviceMemoryAllocationHandle deviceMemoryPool, uint64 deviceMemoryPoolSize)
{
	// TODO: State asserts

	this->device = &device;
	this->deviceMemoryPool = deviceMemoryPool;
	this->deviceMemoryPoolSize = deviceMemoryPoolSize;
}

GraphExecutionContext::GraphExecutionContext(
	HAL::Device& device,
	const Graph& graph,
	HAL::DescriptorAllocatorHandle transientDescriptorAllocator,
	TransientUploadMemoryAllocator& transientUploadMemoryAllocator) :
	device(device),
	graph(graph),
	transientDescriptorAllocator(transientDescriptorAllocator),
	transientUploadMemoryAllocator(transientUploadMemoryAllocator)
{ }

HAL::DescriptorSet GraphExecutionContext::allocateDescriptorSet(HAL::DescriptorSetLayoutHandle descriptorSetLayout)
{
	return device.allocateDescriptorSet(transientDescriptorAllocator, descriptorSetLayout);
}

UploadBufferPointer GraphExecutionContext::allocateUploadMemory(uint32 size)
{
	return transientUploadMemoryAllocator.allocate(size);
}

HAL::BufferHandle GraphExecutionContext::resolveBuffer(BufferHandle bufferHandle)
{
	const uint16 resourceIndex = uint16(bufferHandle);
	XEAssert(resourceIndex < graph.resourceCount);
	const Graph::Resource& resource = graph.resources[resourceIndex];
	XEAssert(resource.type == HAL::ResourceType::Buffer);
	return HAL::BufferHandle(resource.halResourceHandle);
}

HAL::TextureHandle GraphExecutionContext::resolveTexture(TextureHandle textureHandle)
{
	const uint16 resourceIndex = uint16(textureHandle);
	XEAssert(resourceIndex < graph.resourceCount);
	const Graph::Resource& resource = graph.resources[resourceIndex];
	XEAssert(resource.type == HAL::ResourceType::Texture);
	return HAL::TextureHandle(resource.halResourceHandle);
}


// PassDependencyCollector /////////////////////////////////////////////////////////////////////////

PassDependencyCollector::~PassDependencyCollector()
{
	XEMasterAssert(!graph);
}

void PassDependencyCollector::addBufferAcces(BufferHandle bufferHandle,
	HAL::BarrierAccess access, HAL::BarrierSync sync)
{
	// TODO: Should we check something like no write bits set when read is set?
	XEAssert(access != HAL::BarrierAccess::None);
	XEAssert(sync != HAL::BarrierSync::None);

	XEAssert(graph);
	XEAssert(graph->issuedPassDependencyCollector == this);
	XEAssert(graph->passCount > 0);

	const uint16 resourceIndex = uint16(bufferHandle);
	Graph::Resource& resource = graph->resources[resourceIndex];
	XEAssert(resource.type == HAL::ResourceType::Buffer);

	//XEAssert(resource.dependencyCollectorMagic != magic);
	//resource.dependencyCollectorMagic = magic;

	const uint16 passIndex = graph->passCount - 1;
	Graph::Pass& pass = graph->passes[passIndex];
	XEAssert(pass.dependenciesBaseOffset + pass.dependencyCount == graph->dependencyCount);

	const uint16 dependencyIndex = graph->dependencyCount;
	XEMasterAssert(dependencyIndex < graph->dependencyPoolSize);
	graph->dependencyCount++;
	pass.dependencyCount++;

	Graph::Dependency& dependency = graph->dependencies[dependencyIndex];
	dependency = {};
	dependency.resourceIndex = resourceIndex;
	dependency.passIndex = passIndex;
	dependency.access = access;
	dependency.sync = sync;
	dependency.dependsOnPrecedingShaderWrite = true; // ????
	dependency.resourceDependencyChainNextIdx = uint16(-1);

	if (resource.dependencyChainHeadIdx == uint16(-1))
		resource.dependencyChainHeadIdx = dependencyIndex;
	else
		graph->dependencies[resource.dependencyChainTailIdx].resourceDependencyChainNextIdx = dependencyIndex;

	resource.dependencyChainTailIdx = dependencyIndex;
}

void PassDependencyCollector::addTextureAccess(TextureHandle textureHandle,
	HAL::BarrierAccess access, HAL::BarrierSync sync,
	HAL::TextureSubresourceRange* subresourceRange)
{
	XEAssert(graph);
}

void PassDependencyCollector::addBufferShaderRead(BufferHandle bufferHandle, ShaderStage shaderStage)
{
	addBufferAcces(bufferHandle, HAL::BarrierAccess::ShaderReadOnly, TranslateSchedulerShaderStageToHALBarrierSync(shaderStage));
}

void PassDependencyCollector::addBufferShaderWrite(BufferHandle bufferHandle, ShaderStage shaderStage,
	bool dependsOnPrecedingShaderWrite)
{
	XTODO(__FUNCTION__ ": `dependsOnPrecedingShaderWrite` is dropped here");
	addBufferAcces(bufferHandle, HAL::BarrierAccess::ShaderReadWrite, TranslateSchedulerShaderStageToHALBarrierSync(shaderStage));
}

void PassDependencyCollector::close()
{
	XEAssert(graph);
	XEAssert(graph->issuedPassDependencyCollector == this);

	graph->issuedPassDependencyCollector = nullptr;
	graph = nullptr;
}


// Graph ///////////////////////////////////////////////////////////////////////////////////////////

struct Graph::Resource
{
	union
	{
		uint32 bufferSize;
		HAL::TextureDesc textureDesc;
	};
	uint64 transientResourceNameXSH;
	uint32 halResourceHandle;

	HAL::ResourceType type;
	bool isImported;
	//uint8 handleGeneration;

	//uint16 dependencyCollectorMagic;
	uint16 dependencyChainHeadIdx;
	uint16 dependencyChainTailIdx;

	uint16 firstUsagePassIndex;
	uint16 lastUsagePassIndex;

	//uint16 refCount;
};

struct Graph::Pass
{
	PassExecutorFunc executporFunc;
	const void* userData;

	uint16 dependenciesBaseOffset;
	uint16 dependencyCount;

	uint16 preExecutionBarrierChainHeadIdx;
	uint16 postExecutionBarrierChainHeadIdx;

	//uint16 refCount;
};

struct Graph::Dependency
{
	uint16 resourceIndex;
	uint16 passIndex;

	HAL::BarrierAccess access;
	HAL::BarrierSync sync;
	bool dependsOnPrecedingShaderWrite;

	HAL::TextureSubresourceRange textureSubresourceRange;
	bool usesTextureSubresourceRange;

	uint16 resourceDependencyChainNextIdx;
};

struct Graph::Barrier
{
	HAL::BarrierAccess accessBefore;
	HAL::BarrierAccess accessAfter;
	HAL::BarrierSync syncBefore;
	HAL::BarrierSync syncAfter;

	HAL::TextureSubresourceRange textureSubresourceRange;
	HAL::TextureLayout layoutBefore;
	HAL::TextureLayout layoutAfter;

	uint16 resourceIndex;

	uint16 chainNextIdx;
};

void Graph::initialize(HAL::Device& device,
	TransientResourceCache& transientResourceCache,
	const GraphSettings& settings)
{
	XEMasterAssert(!this->device);
	memorySet(this, 0, sizeof(Graph));

	this->device = &device;
	this->transientResourceCache = &transientResourceCache;
	this->resourcePoolSize = settings.resourcePoolSize;
	this->passPoolSize = settings.passPoolSize;
	this->dependencyPoolSize = settings.dependencyPoolSize;
	this->barrierPoolSize = settings.barrierPoolSize;
	this->userDataPoolSize = settings.userDataPoolSize;

	{
		uintptr poolsMemorySizeAccum = 0;

		const uintptr resourcePoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Resource) * resourcePoolSize;

		const uintptr passPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Pass) * passPoolSize;

		const uintptr dependencyPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Dependency) * dependencyPoolSize;

		const uintptr barrierPoolMemoryOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Barrier) * barrierPoolSize;

		const uintptr userDataPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += userDataPoolSize;

		const uintptr poolsTotalMemorySize = poolsMemorySizeAccum;
		poolsMemory = (byte*)XLib::SystemHeapAllocator::Allocate(poolsTotalMemorySize);
		memorySet(poolsMemory, 0, poolsTotalMemorySize);

		resources = (Resource*)(poolsMemory + resourcePoolMemOffset);
		passes = (Pass*)(poolsMemory + passPoolMemOffset);
		dependencies = (Dependency*)(poolsMemory + dependencyPoolMemOffset);
		barriers = (Barrier*)(poolsMemory + barrierPoolMemoryOffset);
		userDataPool = poolsMemory + userDataPoolMemOffset;
	}
}

void Graph::destroy()
{
	if (!device)
		return;

	// TODO: Revisit this.
	XLib::SystemHeapAllocator::Release(poolsMemory);
	memorySet(this, 0, sizeof(Graph));
}

BufferHandle Graph::createTransientBuffer(uint32 size, uint64 nameXSH)
{
	XEAssert(device);
	XEAssert(!isCompiled);

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

TextureHandle Graph::createTransientTexture(HAL::TextureDesc textureDesc, uint64 nameXSH)
{
	XEAssert(device);
	XEAssert(!isCompiled);

	XEMasterAssert(resourceCount < resourcePoolSize);
	const uint16 resourceIndex = resourceCount;
	resourceCount++;

	Resource& resource = resources[resourceIndex];
	resource = {};
	resource.textureDesc = textureDesc;
	resource.transientResourceNameXSH = nameXSH;
	resource.type = HAL::ResourceType::Texture;
	resource.isImported = false;
	resource.dependencyChainHeadIdx = uint16(-1);
	resource.dependencyChainTailIdx = uint16(-1);

	return TextureHandle(resourceIndex);
}

BufferHandle Graph::importExternalBuffer(HAL::BufferHandle bufferHandle)
{
	XEAssert(device);
	XEAssert(!isCompiled);

	XEMasterAssert(resourceCount < resourcePoolSize);
	const uint16 resourceIndex = resourceCount;
	resourceCount++;

	Resource& resource = resources[resourceIndex];
	resource = {};
	// TODO: Fill `resource.bufferSize` just in case.
	resource.halResourceHandle = uint32(bufferHandle);
	resource.type = HAL::ResourceType::Buffer;
	resource.isImported = true;
	resource.dependencyChainHeadIdx = uint16(-1);
	resource.dependencyChainTailIdx = uint16(-1);

	return BufferHandle(resourceIndex);
}

TextureHandle Graph::importExternalTexture(HAL::TextureHandle textureHandle, HAL::TextureLayout defaultLayout)
{
	XEAssert(device);
	XEAssert(!isCompiled);

	XEMasterAssert(resourceCount < resourcePoolSize);
	const uint16 resourceIndex = resourceCount;
	resourceCount++;

	Resource& resource = resources[resourceIndex];
	resource = {};
	// TODO: Fill `resource.textureDesc` just in case.
	resource.halResourceHandle = uint32(textureHandle);
	resource.type = HAL::ResourceType::Texture;
	resource.isImported = true;
	resource.dependencyChainHeadIdx = uint16(-1);
	resource.dependencyChainTailIdx = uint16(-1);

	XEAssert(defaultLayout == HAL::TextureLayout::Common);

	return TextureHandle(resourceIndex);
}

void Graph::addPass(PassType type, PassDependencyCollector& dependencyCollector,
	PassExecutorFunc executorFunc, const void* userData)
{
	XEAssert(device);
	XEAssert(!isCompiled);
	XEAssert(!issuedPassDependencyCollector);

	XEMasterAssert(passCount < passPoolSize);
	const uint16 passIndex = passCount;
	passCount++;

	Pass& pass = passes[passIndex];
	pass.executporFunc = executorFunc;
	pass.userData = userData;
	pass.dependenciesBaseOffset = dependencyCount;
	pass.dependencyCount = 0;
	//pass.refCount = 0;

	XEAssert(!dependencyCollector.graph);
	dependencyCollector.graph = this;
	issuedPassDependencyCollector = &dependencyCollector;
}

void Graph::compile()
{
	XEAssert(device);
	XEAssert(!isCompiled);
	XEAssert(!issuedPassDependencyCollector);
	XEAssert(passCount > 0);

#if 0
	// Compute refcount for resources and passes.
	// Simple graph flood-fill from unreferenced resources.
	// "FrameGraph: Extensible Rendering Architecture in Frostbite" by Yuriy O'Donnell
	{
		for (uint16 i = 0; i < dependencyCount; i++)
		{
			const Dependency& dependency = dependencies[i];

			if (HAL::BarrierAccessUtils::IsReadOnly(dependency.access))
				resources[dependency.resourceIndex].refCount++;
			else
				passes[dependency.passIndex].refCount++;
		}

		XLib::InplaceArrayList<uint16, MaxResourceCount> unreferencedResourcesStack;
		for (uint16 i = 0; i < resourceCount; i++)
		{
			if (resources[i].refCount == 0)
				unreferencedResourcesStack.pushBack(i);
		}

		while (!unreferencedResourcesStack.isEmpty())
		{
			const uint16 unrefResIndex = unreferencedResourcesStack.popBack();

			for (uint16 unrefResDepChainIter = resources[unrefResIndex].dependencyChainHeadIdx;
				unrefResDepChainIter != uint16(-1);
				unrefResDepChainIter = dependencies[unrefResDepChainIter].resourceDependencyChainNextIdx)
			{
				//if (!IsModifyingAccess(dependencies[unrefResDepChainIter].resourceAccessType))
				if (HAL::BarrierAccessUtils::IsReadOnly(dependencies[unrefResDepChainIter].access))
					continue;

				Pass& unrefPass = passes[dependencies[unrefResDepChainIter].passIndex];
				XEAssert(unrefPass.refCount > 0);
				unrefPass.refCount--;
				if (unrefPass.refCount > 0)
					continue;

				for (uint16 i = 0; i < unrefPass.dependencyCount; i++)
				{
					const Dependency& unrefDep = dependencies[unrefPass.dependenciesBaseOffset + i];
					//if (!IsModifyingAccess(unrefDep.resourceAccessType))
					if (HAL::BarrierAccessUtils::IsReadOnly(unrefDep.access))
					{
						Resource& newUnrefRes = resources[unrefDep.resourceIndex];
						XEAssert(newUnrefRes.refCount > 0);
						newUnrefRes.refCount--;

						if (newUnrefRes.refCount == 0)
							unreferencedResourcesStack.pushBack(unrefDep.resourceIndex);
					}
				}
			}
		}
	}
#endif


	// Generate resource barriers.
	for (uint16 resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
	{
		const Resource& resource = resources[resourceIndex];
		//if (resource.refCount == 0)
		//	continue;
		//XEAssert(resource.dependencyChainHeadIdx != uint16(-1));

		//if (resource.type == HAL::ResourceType::Buffer)
		{
			Barrier* prevBarrier = nullptr;
			HAL::BarrierAccess prevBarrierAccess = HAL::BarrierAccess::None;
			HAL::BarrierSync prevBarrierSync = HAL::BarrierSync::None;

			// (T_T)
			HAL::TextureLayout prevTextureLayout = HAL::TextureLayout::Common;

			for (uint16 depChainIter = resource.dependencyChainHeadIdx;
				depChainIter != uint16(-1);
				depChainIter = dependencies[depChainIter].resourceDependencyChainNextIdx)
			{
				const Dependency& dependency = dependencies[depChainIter];
				Pass& pass = passes[dependency.passIndex];

				//if (pass.refCount == 0)
				//	continue;

				const bool readToReadTransition =
					prevBarrierAccess != HAL::BarrierAccess::None &&
					HAL::BarrierAccessUtils::IsReadOnly(prevBarrierAccess) &&
					HAL::BarrierAccessUtils::IsReadOnly(dependency.access);

				const bool shaderWriteToShaderWriteWithoutSync =
					!dependency.dependsOnPrecedingShaderWrite &&
					prevBarrierAccess == HAL::BarrierAccess::ShaderReadWrite &&
					dependency.access == HAL::BarrierAccess::ShaderReadWrite;

				if (readToReadTransition || shaderWriteToShaderWriteWithoutSync)
				{
					// Extend already emitted barrier.
					prevBarrierAccess |= dependency.access;
					prevBarrierSync |= dependency.sync;

					XEAssert(prevBarrier);
					prevBarrier->accessAfter |= dependency.access;
					prevBarrier->syncAfter |= dependency.sync;
				}
				else
				{
					// Generate new barrier.
					XEMasterAssert(barrierCount < barrierPoolSize);
					const uint16 newBarrierIndex = barrierCount;
					barrierCount++;

					Barrier& newBarrier = barriers[newBarrierIndex];
					newBarrier = {};
					newBarrier.resourceIndex = resourceIndex;
					newBarrier.accessBefore = prevBarrierAccess;
					newBarrier.accessAfter = dependency.access;
					newBarrier.syncBefore = prevBarrierSync;
					newBarrier.syncAfter = dependency.sync;
					newBarrier.chainNextIdx = uint16(-1);

					if (resource.type == HAL::ResourceType::Texture)
					{
						newBarrier.layoutBefore = prevTextureLayout;
						newBarrier.layoutAfter = ...;
						...;
					}

					if (prevBarrier)
						prevBarrier->chainNextIdx = newBarrierIndex;
					else
						pass.preExecutionBarrierChainHeadIdx = newBarrierIndex;

					prevBarrier = &newBarrier;
					prevBarrierAccess = dependency.access;
					prevBarrierSync = dependency.sync;
				}
			}

			// TODO: Generate last barrier to NONE state (will need this for aliasing to work properly

			XTODO(__FUNCTION__ ": ");
		}
		/*else if (resource.type == HAL::ResourceType::Texture)
		{

		}
		else
			XEAssertUnreachableCode();*/
	}


	// Compute transient resource lifetime conflict matrix (if we ever need one).
	// ...


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
					resource.firstUsagePassIndex <= alreadyPlacedResource.lastUsagePassIndex &&
					resource.lastUsagePassIndex  >= alreadyPlacedResource.firstUsagePassIndex;

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

	uint32 transientResourceHeapOffsetAccum = 0;
	for (uint16 resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
	{
		const Resource resource = resources[resourceIndex];
		if (resource.isImported)
			continue;

		TransientResourceLocation& resourceLocation = transientResourceLocations[resourceIndex];

		if (resource.type == HAL::ResourceType::Buffer)
		{
			resourceLocation.offset = transientResourceHeapOffsetAccum;
			resourceLocation.size = XCheckedCastU16(
				divRoundUp<uint64>(resource.bufferSize, TransientResourceCache::AllocationAlignment));
		}
		else if (resource.type == HAL::ResourceType::Texture)
		{
			const HAL::ResourceAllocationInfo textureAllocationInfo =
				device->getTextureAllocationInfo(resource.textureDesc);
			XEAssert(textureAllocationInfo.alignment <= TransientResourceCache::AllocationAlignment);

			resourceLocation.offset = transientResourceHeapOffsetAccum;
			resourceLocation.size = XCheckedCastU16(
				divRoundUp<uint64>(textureAllocationInfo.size, TransientResourceCache::AllocationAlignment));
		}

		transientResourceHeapOffsetAccum += resourceLocation.size;
	}

#endif


	// Allocate transient resources.

	// NOTE: This is a placeholder code.
	uint32 transientResourceHeapOffsetAccum = 0;
	for (uint16 resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
	{
		Resource resource = resources[resourceIndex];
		if (resource.isImported)
			continue;

		if (resource.type == HAL::ResourceType::Buffer)
		{
			const HAL::BufferHandle bufferHandle = device->createBuffer(resource.bufferSize);
			resource.halResourceHandle = uint32(bufferHandle);
		}
		else if (resource.type == HAL::ResourceType::Texture)
		{
			const HAL::TextureHandle textureHandle = device->createTexture(resource.textureDesc, HAL::TextureLayout::Common);
			resource.halResourceHandle = uint32(textureHandle);
		}
	}

	isCompiled = true;
}

void Graph::execute(HAL::CommandAllocatorHandle commandAllocator,
	HAL::DescriptorAllocatorHandle transientDescriptorAllocator,
	TransientUploadMemoryAllocator& transientUploadMemoryAllocator) const
{
	XEAssert(device);
	XEAssert(isCompiled);

	GraphExecutionContext context(*device, *this,
		transientDescriptorAllocator, transientUploadMemoryAllocator);

	HAL::CommandList commandList;
	device->openCommandList(commandList, commandAllocator);

	for (uint16 passIndex = 0; passIndex < passCount; passIndex++)
	{
		const Pass& pass = passes[passIndex];
		pass.executporFunc(context, *device, commandList, pass.userData);

		// Emit barriers.
		...;
	}

	device->closeCommandList(commandList);
	device->submitCommandList(HAL::DeviceQueue::Graphics, commandList);

	const HAL::DeviceQueueSyncPoint sp = device->getEndOfQueueSyncPoint(HAL::DeviceQueue::Graphics);
	transientUploadMemoryAllocator.enqueueRelease(sp);
	while (!device->isQueueSyncPointReached(sp))
		{ }
}

void Graph::reset()
{
	XEAssert(device);
	// Graph re-recoding and transient resource reuse is not implemented for now.
	XEAssertUnreachableCode();
}
