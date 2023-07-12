#include "XEngine.Render.Scheduler.h"

using namespace XLib; // TODO: Remove
using namespace XEngine;
using namespace XEngine::Render;
using namespace XEngine::Render::Scheduler;

enum class TransientResourcePool::EntryType : uint8
{
	Undefined = 0,
	Buffer,
	Texture,
	Descriptor,
	RenderTarget,
	DepthRenderTarget,
};

void TransientResourcePool::initialize(GfxHAL::Device& device,
	GfxHAL::DeviceMemoryAllocationHandle deviceMemoryPool, uint64 deviceMemoryPoolSize)
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
				device->destroyTexture(entry.texture.textureHandle);
				entry.texture.textureHandle = GfxHAL::TextureHandle::Zero;
				break;

			case EntryType::Descriptor:
				XAssertUnreachableCode();
				break;

			case EntryType::RenderTarget:
				device->destroyRenderTargetView(entry.renderTarget.renderTargetViewHandle);
				entry.renderTarget.renderTargetViewHandle = GfxHAL::RenderTargetViewHandle::Zero;
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

GfxHAL::DescriptorAddress PassExecutionContext::allocateTransientDescriptors(uint16 descriptorCount)
{
	// TODO: Revisit this
	return transientDescriptorAllocator.allocate(descriptorCount);
}

GfxHAL::DescriptorSetReference PassExecutionContext::allocateTransientDescriptorSet(
	GfxHAL::DescriptorSetLayoutHandle descriptorSetLayout)
{
	const uint16 descriptorCount = device.getDescriptorSetLayoutDescriptorCount(descriptorSetLayout);
	const GfxHAL::DescriptorAddress descriptorSetBaseAddress = allocateTransientDescriptors(descriptorCount);
	return device.createDescriptorSetReference(descriptorSetLayout, descriptorSetBaseAddress);
}

UploadMemoryAllocationInfo PassExecutionContext::allocateTransientUploadMemory(uint32 size)
{
	return transientUploadMemoryAllocator.allocate(size);
}

GfxHAL::TextureHandle PassExecutionContext::resolveTexture(Scheduler::TextureHandle texture)
{
	const uint32 resourceIndex = uint32(texture);
	XAssert(resourceIndex < schedule.resources.getSize());
	const Schedule::Resource& resource = schedule.resources[resourceIndex];
	XAssert(resource.type == GfxHAL::ResourceType::Texture);

	if (resource.lifetime == Schedule::ResourceLifetime::Transient)
	{
		XAssert(resource.transientResourcePoolEntryIndex < transientResourcePool.entries.getSize());
		const TransientResourcePool::Entry& transientResourcePoolEntry =
			transientResourcePool.entries[resource.transientResourcePoolEntryIndex];
		XAssert(transientResourcePoolEntry.type == TransientResourcePool::EntryType::Texture);
		return transientResourcePoolEntry.texture.textureHandle;
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

GfxHAL::RenderTargetViewHandle PassExecutionContext::createTransientRenderTargetView(GfxHAL::TextureHandle texture,
	GfxHAL::TexelViewFormat format, uint8 mipLevel, uint16 arrayIndex)
{
	const GfxHAL::RenderTargetViewHandle rtv = device.createRenderTargetView(texture, format, mipLevel, arrayIndex);

	TransientResourcePool::Entry transientResourcePoolEntry = {};
	transientResourcePoolEntry.type = TransientResourcePool::EntryType::RenderTarget;
	transientResourcePoolEntry.renderTarget.renderTargetViewHandle = rtv;
	transientResourcePool.entries.pushBack(transientResourcePoolEntry);

	return rtv;
}

/*DepthStencilViewHandle PassExecutionContext::createTransientDepthStencilView(Scheduler::TextureHandle texture,
	bool writableDepth, bool writableStencil, uint8 mipLevel, uint16 arrayIndex) { }*/

void Schedule::initialize(GfxHAL::Device& device)
{
	XAssert(!this->device);

	this->device = &device;
	device.createCommandList(commandList, GfxHAL::CommandListType::Graphics);
}

//Scheduler::BufferHandle Schedule::createTransientBuffer(uint32 size) { }

/*Scheduler::TextureHandle Schedule::createTransientTexture(const GfxHAL::TextureDesc& textureDesc)
{
	Resource resource = {};
	resource.type = GfxHAL::ResourceType::Texture;
}*/

Scheduler::TextureHandle Schedule::importExternalTexture(GfxHAL::TextureHandle texture)
{
	Resource resource = {};
	resource.type = GfxHAL::ResourceType::Texture;
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

void Schedule::execute(TransientResourcePool& transientResourcePool,
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

		if (resource.type == GfxHAL::ResourceType::Buffer)
		{
			const uint64 requiredMemorySize = alignUp<uint64>(resource.transientBufferSize, 65536);
			const uint64 memoryOffset = transientResourcePoolMemoryUsed;
			transientResourcePoolMemoryUsed += requiredMemorySize;

			const GfxHAL::BufferHandle buffer =
				device->createBuffer(resource.transientBufferSize, true,
					// GfxHAL::BufferMemoryType::DeviceLocal, transientResourcePool.deviceMemoryPool, memoryOffset
					GfxHAL::BufferMemoryType::DeviceLocal);

			resource.transientResourcePoolEntryIndex = transientResourcePool.entries.getSize();

			TransientResourcePool::Entry transientResourcePoolEntry = {};
			transientResourcePoolEntry.type = TransientResourcePool::EntryType::Buffer;
			transientResourcePoolEntry.buffer.bufferHandle = buffer;
			transientResourcePool.entries.pushBack(transientResourcePoolEntry);
		}
		else if (resource.type == GfxHAL::ResourceType::Texture)
		{
			const GfxHAL::ResourceAllocationInfo textureAllocationInfo =
				device->getTextureAllocationInfo(resource.transientTextureDesc);

			const uint64 requiredMemorySize = alignUp<uint64>(textureAllocationInfo.size, 65536); // TODO: Do we need alignUp here?
			const uint64 memoryOffset = transientResourcePoolMemoryUsed;
			transientResourcePoolMemoryUsed += requiredMemorySize;

			const GfxHAL::TextureHandle texture =
				device->createTexture(resource.transientTextureDesc,
					//transientResourcePool.deviceMemoryPool, memoryOffset);
					GfxHAL::DeviceMemoryAllocationHandle::Zero);

			resource.transientResourcePoolEntryIndex = transientResourcePool.entries.getSize();

			TransientResourcePool::Entry transientResourcePoolEntry = {};
			transientResourcePoolEntry.type = TransientResourcePool::EntryType::Texture;
			transientResourcePoolEntry.texture.textureHandle = texture;
			transientResourcePool.entries.pushBack(transientResourcePoolEntry);
		}
	}

	PassExecutionContext context(*device, *this,
		transientResourcePool, transientDescriptorAllocator, transientUploadMemoryAllocator);

	commandList.open();

	for (Pass& pass : passes)
		pass.executporFunc(context, *device, commandList, pass.userData);

	//commandList.close();

	device->submitWorkload(GfxHAL::DeviceQueue::Main, commandList);

	const GfxHAL::DeviceQueueSyncPoint sp = device->getEndOfQueueSyncPoint(GfxHAL::DeviceQueue::Main);

	transientDescriptorAllocator.enqueueRelease(sp);
	transientUploadMemoryAllocator.enqueueRelease(sp);

	while (!device->isQueueSyncPointReached(sp))
		{ }

	transientResourcePool.reset();

	resources.clear();
	passes.clear();
}
