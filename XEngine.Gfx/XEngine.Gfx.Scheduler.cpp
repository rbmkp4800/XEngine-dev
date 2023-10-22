#include "XEngine.Gfx.Scheduler.h"

using namespace XEngine::Gfx;
using namespace XEngine::Gfx::Scheduler;

enum class TransientResourcePool::EntryType : uint8
{
	Undefined = 0,
	Buffer,
	Texture,
	Descriptor,
	RenderTarget,
	DepthRenderTarget,
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

			case EntryType::Descriptor:
				XAssertUnreachableCode();
				break;

			case EntryType::RenderTarget:
				device->destroyColorRenderTarget(entry.colorRenderTarget.handle);
				entry.colorRenderTarget.handle = HAL::ColorRenderTargetHandle::Zero;
				break;

			case EntryType::DepthRenderTarget:
				XAssertUnreachableCode();
				break;

			default:
				XAssertUnreachableCode();
		}
	}
	entries.clear();
}

HAL::DescriptorAddress PassExecutionContext::allocateTransientDescriptors(uint16 descriptorCount)
{
	// TODO: Revisit this
	return transientDescriptorAllocator.allocate(descriptorCount);
}

HAL::DescriptorSetReference PassExecutionContext::allocateTransientDescriptorSet(
	HAL::DescriptorSetLayoutHandle descriptorSetLayout)
{
	const uint16 descriptorCount = device.getDescriptorSetLayoutDescriptorCount(descriptorSetLayout);
	const HAL::DescriptorAddress descriptorSetBaseAddress = allocateTransientDescriptors(descriptorCount);
	return device.createDescriptorSetReference(descriptorSetLayout, descriptorSetBaseAddress);
}

UploadMemoryAllocationInfo PassExecutionContext::allocateTransientUploadMemory(uint32 size)
{
	return transientUploadMemoryAllocator.allocate(size);
}

HAL::TextureHandle PassExecutionContext::resolveTexture(Scheduler::TextureHandle texture)
{
	const uint32 resourceIndex = uint32(texture);
	XAssert(resourceIndex < schedule.resources.getSize());
	const Schedule::Resource& resource = schedule.resources[resourceIndex];
	XAssert(resource.type == HAL::ResourceType::Texture);

	if (resource.lifetime == Schedule::ResourceLifetime::Transient)
	{
		XAssert(resource.transientResourcePoolEntryIndex < transientResourcePool.entries.getSize());
		const TransientResourcePool::Entry& transientResourcePoolEntry =
			transientResourcePool.entries[resource.transientResourcePoolEntryIndex];
		XAssert(transientResourcePoolEntry.type == TransientResourcePool::EntryType::Texture);
		return transientResourcePoolEntry.texture.handle;
	}
	else
	{
		XAssert(resource.lifetime == Schedule::ResourceLifetime::External);
		return resource.externalTextureHandle;
	}
}

/*ResourceViewHandle PassExecutionContext::createTransientBufferView(Scheduler::BufferHandle buffer) { }
ResourceViewHandle PassExecutionContext::createTransientTextureView(Scheduler::TextureHandle texture,
	TexelViewFormat format, bool writable, const TextureSubresourceRange& subresourceRange) { }*/

HAL::ColorRenderTargetHandle PassExecutionContext::createTransientColorRenderTarget(HAL::TextureHandle texture,
	HAL::TexelViewFormat format, uint8 mipLevel, uint16 arrayIndex)
{
	const HAL::ColorRenderTargetHandle rt = device.createColorRenderTarget(texture, format, mipLevel, arrayIndex);

	TransientResourcePool::Entry transientResourcePoolEntry = {};
	transientResourcePoolEntry.type = TransientResourcePool::EntryType::RenderTarget;
	transientResourcePoolEntry.colorRenderTarget.handle = rt;
	transientResourcePool.entries.pushBack(transientResourcePoolEntry);

	return rt;
}

/*DepthStencilViewHandle PassExecutionContext::createTransientDepthStencilView(Scheduler::TextureHandle texture,
	bool writableDepth, bool writableStencil, uint8 mipLevel, uint16 arrayIndex) { }*/

void Schedule::initialize(HAL::Device& device)
{
	XAssert(!this->device);

	this->device = &device;
}

//Scheduler::BufferHandle Schedule::createTransientBuffer(uint32 size) { }

/*Scheduler::TextureHandle Schedule::createTransientTexture(const HAL::TextureDesc& textureDesc)
{
	Resource resource = {};
	resource.type = HAL::ResourceType::Texture;
}*/

Scheduler::TextureHandle Schedule::importExternalTexture(HAL::TextureHandle texture)
{
	Resource resource = {};
	resource.type = HAL::ResourceType::Texture;
	resource.lifetime = ResourceLifetime::External;
	resource.externalTextureHandle = texture;
	
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

void Schedule::execute(HAL::CommandAllocatorHandle commandAllocator,
	TransientResourcePool& transientResourcePool,
	TransientDescriptorAllocator& transientDescriptorAllocator,
	TransientUploadMemoryAllocator& transientUploadMemoryAllocator)
{
	XEAssert(device);
	// TODO: Assert that transient resource pool created for same device? Same for descriptors and upload memory allocators.

	XAssert(transientResourcePool.entries.isEmpty());

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

	PassExecutionContext context(*device, *this,
		transientResourcePool, transientDescriptorAllocator, transientUploadMemoryAllocator);

	HAL::CommandList commandList;

	device->openCommandList(commandList, commandAllocator);

	for (Pass& pass : passes)
		pass.executporFunc(context, *device, commandList, pass.userData);

	device->closeCommandList(commandList);
	device->submitCommandList(HAL::DeviceQueue::Graphics, commandList);

	const HAL::DeviceQueueSyncPoint sp = device->getEndOfQueueSyncPoint(HAL::DeviceQueue::Graphics);

	transientDescriptorAllocator.enqueueRelease(sp);
	transientUploadMemoryAllocator.enqueueRelease(sp);

	while (!device->isQueueSyncPointReached(sp))
		{ }

	transientResourcePool.reset();

	resources.clear();
	passes.clear();
}
