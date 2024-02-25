#include "XEngine.Gfx.Scheduler.h"

using namespace XEngine::Gfx;
using namespace XEngine::Gfx::Scheduler;

enum class TransientResourcePool::EntryType : uint8
{
	Undefined = 0,
	Buffer,
	Texture,
};

void TransientResourcePool::initialize(HAL::Device& device,
	HAL::DeviceMemoryAllocationHandle deviceMemoryPool, uint64 deviceMemoryPoolSize)
{
	// TODO: State asserts

	this->device = &device;
	this->deviceMemoryPool = deviceMemoryPool;
	this->deviceMemoryPoolSize = deviceMemoryPoolSize;
}

void TransientResourcePool::reset()
{
	for (Entry& entry : entries)
	{
		switch (entry.type)
		{
			case EntryType::Buffer:
				XAssertUnreachableCode();
				break;

			case EntryType::Texture:
				device->destroyTexture(entry.texture.handle);
				entry.texture.handle = HAL::TextureHandle::Zero;
				break;

			default:
				XAssertUnreachableCode();
		}
	}
	entries.clear();
}

PassExecutionContext::PassExecutionContext(HAL::Device& device, const Schedule& schedule, HAL::DescriptorAllocatorHandle transientDescriptorAllocator,
	TransientUploadMemoryAllocator& transientUploadMemoryAllocator, TransientResourcePool& transientResourcePool) :
	device(device),
	schedule(schedule),
	transientDescriptorAllocator(transientDescriptorAllocator),
	transientUploadMemoryAllocator(transientUploadMemoryAllocator),
	transientResourcePool(transientResourcePool)
{ }

HAL::DescriptorSet PassExecutionContext::allocateDescriptorSet(HAL::DescriptorSetLayoutHandle descriptorSetLayout)
{
	return device.allocateDescriptorSet(transientDescriptorAllocator, descriptorSetLayout);
}

UploadMemoryAllocationInfo PassExecutionContext::allocateUploadMemory(uint32 size)
{
	return transientUploadMemoryAllocator.allocate(size);
}

HAL::TextureHandle PassExecutionContext::resolveTexture(Scheduler::TextureHandle texture)
{
	const uint32 resourceIndex = uint32(texture);
	XEAssert(resourceIndex < schedule.resources.getSize());
	const Schedule::Resource& resource = schedule.resources[resourceIndex];
	XAssert(resource.type == HAL::ResourceType::Texture);

	if (resource.lifetime == Schedule::ResourceLifetime::Transient)
	{
		XEAssert(resource.transientResourcePoolEntryIndex < transientResourcePool.entries.getSize());
		const TransientResourcePool::Entry& transientResourcePoolEntry =
			transientResourcePool.entries[resource.transientResourcePoolEntryIndex];
		XEAssert(transientResourcePoolEntry.type == TransientResourcePool::EntryType::Texture);
		return transientResourcePoolEntry.texture.handle;
	}
	else
	{
		XEAssert(resource.lifetime == Schedule::ResourceLifetime::External);
		return resource.externalTextureHandle;
	}
}


////////////////////////////////////////////////////////////////////////////////////////////////////

enum class Schedule::ResourceLifetime : uint8
{
	Undefined = 0,
	Transient,
	External,
};

struct Schedule::Resource
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

struct Schedule::Pass
{
	PassExecutorFunc executporFunc;
	const void* userData;
};

void Schedule::initialize(HAL::Device& device, uint16 maxResourceCount, uint16 maxPassCount)
{
	XAssert(!this->device);
	this->device = &device;
	...;
}

Scheduler::BufferHandle Schedule::createTransientBuffer(uint32 size)
{
	...;
}

Scheduler::TextureHandle Schedule::createTransientTexture(const HAL::TextureDesc& textureDesc)
{
	...;
}

Scheduler::TextureHandle Schedule::importExternalTexture(HAL::TextureHandle texture, HAL::TextureLayout defaultLayout)
{
	Resource resource = {};
	resource.type = HAL::ResourceType::Texture;
	resource.lifetime = ResourceLifetime::External;
	resource.externalTextureHandle = texture;
	defaultLayout ...;
	
	const Scheduler::TextureHandle result = Scheduler::TextureHandle(resources.getSize());
	resources.pushBack(resource);

	return result;
}

void Schedule::addPass(PassType type, const PassDependenciesDesc& dependencies,
	PassExecutorFunc executorFunc, const void* userData)
{
	Pass pass = {};
	pass.executporFunc = executorFunc;
	pass.userData = userData;
	passes.pushBack(pass);
}

void Schedule::compile()
{

}

void Schedule::execute(HAL::CommandAllocatorHandle commandAllocator, HAL::DescriptorAllocatorHandle transientDescriptorAllocator,
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
