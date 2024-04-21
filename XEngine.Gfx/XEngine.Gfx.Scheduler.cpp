#include <XLib.Containers.ArrayList.h>
#include <XLib.SystemHeapAllocator.h>

#include "XEngine.Gfx.Scheduler.h"

using namespace XEngine::Gfx;
using namespace XEngine::Gfx::Scheduler;


// TransientResourcePool ///////////////////////////////////////////////////////////////////////////

enum class TransientResourcePool::EntryType : uint8
{
	Undefined = 0,
	Buffer,
	Texture,
};

struct TransientResourcePool::Entry
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

void TransientResourcePool::initialize(HAL::Device& device,
	HAL::DeviceMemoryAllocationHandle deviceMemoryPool, uint64 deviceMemoryPoolSize)
{
	// TODO: State asserts

	this->device = &device;
	this->deviceMemoryPool = deviceMemoryPool;
	this->deviceMemoryPoolSize = deviceMemoryPoolSize;
}

PassExecutionContext::PassExecutionContext(HAL::Device& device, const Graph& graph, HAL::DescriptorAllocatorHandle descriptorAllocator,
	TransientUploadMemoryAllocator& transientUploadMemoryAllocator, TransientResourcePool& transientResourcePool) :
	device(device),
	graph(graph),
	descriptorAllocator(descriptorAllocator),
	transientUploadMemoryAllocator(transientUploadMemoryAllocator),
	transientResourcePool(transientResourcePool)
{ }

HAL::DescriptorSet PassExecutionContext::allocateDescriptorSet(HAL::DescriptorSetLayoutHandle descriptorSetLayout)
{
	return device.allocateDescriptorSet(descriptorAllocator, descriptorSetLayout);
}

UploadMemoryAllocationInfo PassExecutionContext::allocateUploadMemory(uint32 size)
{
	return transientUploadMemoryAllocator.allocate(size);
}

HAL::TextureHandle PassExecutionContext::resolveTexture(Scheduler::TextureHandle texture)
{
	const uint32 resourceIndex = uint32(texture);
	XEAssert(resourceIndex < graph.resources.getSize());
	const Graph::Resource& resource = graph.resources[resourceIndex];
	XAssert(resource.type == HAL::ResourceType::Texture);

	if (resource.lifetime == Graph::ResourceLifetime::Transient)
	{
		XEAssert(resource.transientResourcePoolEntryIndex < transientResourcePool.entries.getSize());
		const TransientResourcePool::Entry& transientResourcePoolEntry =
			transientResourcePool.entries[resource.transientResourcePoolEntryIndex];
		XEAssert(transientResourcePoolEntry.type == TransientResourcePool::EntryType::Texture);
		return transientResourcePoolEntry.texture.handle;
	}
	else
	{
		XEAssert(resource.lifetime == Graph::ResourceLifetime::External);
		return resource.externalTextureHandle;
	}
}


// PassDependencyCollector /////////////////////////////////////////////////////////////////////////

PassDependencyCollector::~PassDependencyCollector()
{
	XEMasterAssert(!graph);
}

void PassDependencyCollector::addBufferShaderRead(BufferHandle bufferHandle)
{
	XEAssert(graph);

	const uint16 resourceIndex = ...;
	const uint16 passIndex = graph->passCount;

	Graph::Resource& resource = graph->resources[resourceIndex];
	XEAssert(resource.type == HAL::ResourceType::Buffer);

	XEAssert(resource.dependencyCollectorMagic != magic);
	resource.dependencyCollectorMagic = magic;

	const uint16 globalDependencyIndex = graph->dependencyCount + dependencyCount;
	XEMasterAssert(globalDependencyIndex < graph->dependencyPoolSize);
	dependencyCount++;

	Graph::Dependency& dependency = graph->dependencies[globalDependencyIndex];
	dependency.resourceIndex = resourceIndex;
	dependency.passIndex = passIndex;
	dependency.resourceAccessType = Graph::ResourceAccessType::ShaderRead;
}

void PassDependencyCollector::addBufferShaderWrite(BufferHandle bufferHandle, BufferHandle& modifiedBufferHandle)
{

}


// Graph ///////////////////////////////////////////////////////////////////////////////////////////

enum class Graph::ResourceOrigin : uint8
{
	Undefined = 0,
	Transient,
	External,
};

enum class Graph::ResourceAccessType : uint8
{
	Undefined = 0,
	ShaderRead,
	ShaderWrite,
	ColorRenderTarget,
	DepthStencilRenderTargetRead,
	DepthStencilRenderTargetWrite,
};

struct Graph::Resource
{
	union
	{
		uint32 transientBufferSize;
		HAL::TextureDesc transientTextureDesc;
		HAL::BufferHandle externalBufferHandle;
		HAL::TextureHandle externalTextureHandle;
	};

	//uint32 transientResourcePoolEntryIndex;

	HAL::ResourceType type;
	ResourceOrigin origin;
	uint8 handleGeneration;

	//uint16 dependencyCollectorMagic;
	uint16 dependencyChainHeadIdx;

	uint16 refCount;
};

struct Graph::Pass
{
	PassExecutorFunc executporFunc;
	const void* userData;

	uint16 dependenciesBaseOffset;
	uint16 dependencyCount;

	uint16 refCount;
};

struct Graph::Dependency
{
	uint16 resourceIndex;
	uint16 passIndex;
	ResourceAccessType resourceAccessType;

	HAL::TextureSubresourceRange textureSubresourceRange;
	bool usesTextureSubresourceRange;

	uint16 resourceDependencyChainNextIdx;
};

inline bool Graph::IsModifyingAccess(ResourceAccessType access)
{
	return
		access == ResourceAccessType::ShaderWrite ||
		access == ResourceAccessType::ColorRenderTarget ||
		access == ResourceAccessType::DepthStencilRenderTargetWrite;
}

void Graph::initialize(HAL::Device& device, uint16 resourcePoolSize, uint16 passPoolSize, uint16 dependencyPoolSize, uint32 userDataPoolSize)
{
	XEMasterAssert(!this->device);
	memorySet(this, 0, sizeof(Graph));

	this->device = &device;
	this->resourcePoolSize = resourcePoolSize;
	this->passPoolSize = passPoolSize;
	this->dependencyPoolSize = dependencyPoolSize;
	this->userDataPoolSize = userDataPoolSize;

	{
		uintptr poolsMemorySizeAccum = 0;

		const uintptr resourcePoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Resource) * resourcePoolSize;

		const uintptr passPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Pass) * passPoolSize;

		const uintptr dependenciesPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += sizeof(Dependency) * dependencyPoolSize;

		const uintptr userDataPoolMemOffset = poolsMemorySizeAccum;
		poolsMemorySizeAccum += userDataPoolSize;

		const uintptr poolsTotalMemorySize = poolsMemorySizeAccum;
		poolsMemory = (byte*)XLib::SystemHeapAllocator::Allocate(poolsTotalMemorySize);
		memorySet(poolsMemory, 0, poolsTotalMemorySize);

		resources = (Resource*)(poolsMemory + resourcePoolMemOffset);
		passes = (Pass*)(poolsMemory + passPoolMemOffset);
		dependencies = (Dependency*)(poolsMemory + dependenciesPoolMemOffset);
		userDataPool = poolsMemory + userDataPoolMemOffset;
	}
}

void Graph::destroy()
{
	if (!device)
		return;

	XLib::SystemHeapAllocator::Release(poolsMemory);
	poolsMemory = nullptr;

	// TODO: Maybe some checks & cleanup?

	device = nullptr;
}

Scheduler::BufferHandle Graph::createTransientBuffer(uint32 size)
{
	XEMasterAssert(resourceCount < resourcePoolSize);
	const uint16 resourceIndex = resourceCount;
	resourceCount++;

	Resource& resource = resources[resourceIndex];
	resource.type = HAL::ResourceType::Buffer;
	resource.origin = ResourceOrigin::Transient;
	resource.transientBufferSize = size;
	resource. ... = ...;

	return ComposeBufferHandle(resourceIndex);
}

Scheduler::TextureHandle Graph::createTransientTexture(const HAL::TextureDesc& textureDesc)
{
	...;
}

Scheduler::TextureHandle Graph::importExternalTexture(HAL::TextureHandle texture, HAL::TextureLayout defaultLayout)
{
	...;
}

void Graph::issuePassDependencyCollector(PassDependencyCollector& dependencyCollector)
{
	XEMasterAssert(!issuedPassDependencyCollector);
	XEAssert(!dependencyCollector.graph);

	??? issuedPassDependencyCollectorMagic++;
	const uint16 magic = ...;

	dependencyCollector.graph = this;
	dependencyCollector.dependencyCount = 0;
	dependencyCollector.magic = magic;
	issuedPassDependencyCollector = &dependencyCollector;
	issuedPassDependencyCollectorMagic = magic;
}

void Graph::addPass(PassType type, PassDependencyCollector& dependencyCollector,
	PassExecutorFunc executorFunc, const void* userData)
{
	XEAssert(issuedPassDependencyCollector);
	XEMasterAssert(&dependencyCollector == issuedPassDependencyCollector);
	XEMasterAssert(dependencyCollector.magic == issuedPassDependencyCollectorMagic);

	XEMasterAssert(passCount < passPoolSize);
	const uint16 passIndex = passCount;
	passCount++;

	const uint16 dependenciesBaseOffset = dependencyCount;
	dependencyCount += dependencyCollector.dependencyCount;

	Pass& pass = passes[passIndex];
	pass.executporFunc = executorFunc;
	pass.userData = userData;
	pass.dependenciesBaseOffset = dependenciesBaseOffset;
	pass.dependencyCount = dependencyCollector.dependencyCount;
	pass.refCount = 0;

	dependencyCollector.graph = nullptr;
	dependencyCollector.dependencyCount = 0;
	dependencyCollector.magic = 0;
	issuedPassDependencyCollector = nullptr;

	// TODO: Check multiple accesses for same resource.
	...;

	return ComposePassHandle(passIndex);
}

void Graph::compile()
{
	XEAssert(device);

	// Compute refcount for resources and passes.
	// Simple graph flood-fill from unreferenced resources.
	// "FrameGraph: Extensible Rendering Architecture in Frostbite" by Yuriy O'Donnell
	{
		for (uint16 i = 0; i < dependencyCount; i++)
		{
			const Dependency& dependency = dependencies[i];

			if (IsModifyingAccess(dependency.resourceAccessType))
				passes[dependency.passIndex].refCount++;
			else
				resources[dependency.resourceIndex].refCount++;
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
				if (!IsModifyingAccess(dependencies[unrefResDepChainIter].resourceAccessType))
					continue;

				Pass& unrefPass = passes[dependencies[unrefResDepChainIter].passIndex];
				XEAssert(unrefPass.refCount > 0);
				unrefPass.refCount--;
				if (unrefPass.refCount > 0)
					continue;

				for (uint16 i = 0; i < unrefPass.dependencyCount; i++)
				{
					const Dependency& unrefDep = dependencies[unrefPass.dependenciesBaseOffset + i];
					if (!IsModifyingAccess(unrefDep.resourceAccessType))
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

	// Remove unreferenced elements from resource dependency chains.

	// Compute aliased resource locations.

	// Compute first and last use of resource (to do aliasing).

	// Compute resource barriers.
	for (uint16 resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
	{
		const Resource& resource = resources[resourceIndex];
		if (resource.refCount == 0)
			continue;
		XEAssert(resource.dependencyChainHeadIdx != uint16(-1));

		uint16 prevPassGroupTailIdx = uint16(-1);
		uint16 currPassGroupHeadIdx = uint16(-1);

		for (uint16 depChainIter = resource.dependencyChainHeadIdx;
			depChainIter != uint16(-1);
			depChainIter = dependencies[depChainIter].resourceDependencyChainNextIdx)
		{
			const Dependency& dependency = dependencies[depChainIter];
			const Pass& pass = passes[dependency.passIndex];

			if (pass.refCount == 0)
				continue;


		}


		
	}





	/*struct Barrier
	{
		uint16 resourceIndex;

		HAL::BarrierSync syncBefore;
		HAL::BarrierSync syncAfter;
		HAL::BarrierAccess accessBefore;
		HAL::BarrierAccess accessAfter;

		HAL::TextureLayout textureLayoutBefore;
		HAL::TextureLayout textureLayoutAfter;

		HAL::TextureSubresourceRange textureSubresourceRange;

		uint16 prev;
		uint16 next;
	};*/


	// Separate pass to check for possible "early-flushes" of split-barriers.
}

void Graph::execute(HAL::CommandAllocatorHandle commandAllocator, HAL::DescriptorAllocatorHandle descriptorAllocator,
	TransientUploadMemoryAllocator& transientUploadMemoryAllocator, TransientResourcePool& transientResourcePool)
{
	XEAssert(device);

	uint64 transientResourcePoolMemoryUsed = 0;
	for (Resource& resource : resources)
	{
		if (resource.lifetime != ResourceLifetime::Transient)
			continue;

		if (resource.type == HAL::ResourceType::Buffer)
		{
			const uint64 requiredMemorySize = alignUp<uint64>(resource.transientBufferSize, 65536);
			const uint64 memoryOffset = transientResourcePoolMemoryUsed;
			transientResourcePoolMemoryUsed += requiredMemorySize;

			const HAL::BufferHandle buffer =
				device->createBuffer(resource.transientBufferSize);

			resource.transientResourcePoolEntryIndex = transientResourcePool.entries.getSize();

			TransientResourcePool::Entry transientResourcePoolEntry = {};
			transientResourcePoolEntry.type = TransientResourcePool::EntryType::Buffer;
			transientResourcePoolEntry.buffer.handle = buffer;
			transientResourcePool.entries.pushBack(transientResourcePoolEntry);
		}
		else if (resource.type == HAL::ResourceType::Texture)
		{
			const HAL::ResourceAllocationInfo textureAllocationInfo =
				device->getTextureAllocationInfo(resource.transientTextureDesc);

			const uint64 requiredMemorySize = alignUp<uint64>(textureAllocationInfo.size, 65536); // TODO: Do we need alignUp here?
			const uint64 memoryOffset = transientResourcePoolMemoryUsed;
			transientResourcePoolMemoryUsed += requiredMemorySize;

			const HAL::TextureHandle texture =
				device->createTexture(resource.transientTextureDesc, HAL::TextureLayout::Common,
					//transientResourcePool.deviceMemoryPool, memoryOffset);
					HAL::DeviceMemoryAllocationHandle::Zero);

			resource.transientResourcePoolEntryIndex = transientResourcePool.entries.getSize();

			TransientResourcePool::Entry transientResourcePoolEntry = {};
			transientResourcePoolEntry.type = TransientResourcePool::EntryType::Texture;
			transientResourcePoolEntry.texture.handle = texture;
			transientResourcePool.entries.pushBack(transientResourcePoolEntry);
		}
	}

	{
		PassExecutionContext context(*device, *this, transientDescriptorAllocator,
			transientUploadMemoryAllocator, transientResourcePool);

		HAL::CommandList commandList;
		device->openCommandList(commandList, commandAllocator);

		for (Pass& pass : passes)
			pass.executporFunc(context, *device, commandList, pass.userData);

		device->closeCommandList(commandList);
		device->submitCommandList(HAL::DeviceQueue::Graphics, commandList);
	}

	const HAL::DeviceQueueSyncPoint sp = device->getEndOfQueueSyncPoint(HAL::DeviceQueue::Graphics);
	transientUploadMemoryAllocator.enqueueRelease(sp);
	while (!device->isQueueSyncPointReached(sp))
		{ }

	transientResourcePool.reset();

	resources.clear();
	passes.clear();
}
