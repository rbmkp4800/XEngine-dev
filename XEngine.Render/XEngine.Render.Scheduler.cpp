#include <XLib.Containers.ArrayList.h> // TODO: Remove

#include "XEngine.Render.Scheduler.h"

using namespace XLib; // TODO: Remove
using namespace XEngine::Render;
using namespace XEngine::Render::HAL;
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

struct TransientResourcePool::Entry
{
	EntryType type;

	union
	{
		struct
		{
			HAL::BufferHandle buffer;
		} buffer;

		struct
		{
			HAL::TextureHandle texture;
		} texture;

		struct
		{

		} descriptor;

		struct
		{

		} renderTarget;

		struct
		{

		} depthRenderTarget;
	};
};

DescriptorAddress PassExecutionBroker::allocateTransientDescriptors(uint16 descriptorCount)
{
	// TODO: Revisit this
	return DescriptorAddress(transientDescriptorAllocator.allocate(descriptorCount));
}

DescriptorSetReference PassExecutionBroker::allocateTransientDescriptorSet(DescriptorSetLayoutHandle descriptorSetLayout)
{
	const uint32 descriptorCount = device.getDescriptorSetLayoutDescriptorCount(descriptorSetLayout);
	const DescriptorAddress descriptorSetBaseAddress = allocateTransientDescriptors(descriptorCount);
	return device.createDescriptorSetReference(descriptorSetLayout, descriptorSetBaseAddress);
}

UploadMemoryAllocationInfo PassExecutionBroker::allocateTransientUploadMemory(uint32 size)
{
	return transientUploadMemoryAllocator.allocate(size);
}

ResourceViewHandle PassExecutionBroker::createTransientBufferView(Scheduler::BufferHandle buffer)
{

}

ResourceViewHandle PassExecutionBroker::createTransientTextureView(Scheduler::TextureHandle texture,
	TexelViewFormat format, bool writable, const TextureSubresourceRange& subresourceRange)
{

}

RenderTargetViewHandle PassExecutionBroker::createTransientRenderTargetView(Scheduler::TextureHandle texture,
	HAL::TexelViewFormat format, uint8 mipLevel, uint16 arrayIndex)
{

}

DepthStencilViewHandle PassExecutionBroker::createTransientDepthStencilView(Scheduler::TextureHandle texture,
	bool writableDepth, bool writableStencil, uint8 mipLevel, uint16 arrayIndex)
{

}

enum class Pipeline::ResourceLifetime : uint8
{
	Undefined = 0,
	Transient,
	External,
};

struct Pipeline::Resource
{
	ResourceType type;
	ResourceLifetime lifetime;

	union
	{
		uint32 transientBufferSize;
		TextureDesc transientTextureDesc;
		HAL::BufferHandle externalBufferHandle;
		HAL::TextureHandle externalTextureHandle;
	};
};

struct Pipeline::Pass
{
	PassExecutorFunc executporFunc;
	const void* userData;
};

Scheduler::BufferHandle Pipeline::createTransientBuffer(uint32 size)
{

}

Scheduler::TextureHandle Pipeline::createTransientTexture(const TextureDesc& textureDesc)
{

}

void Pipeline::execute(TransientResourcePool& transientResourcePool,
	CircularDescriptorAllocator& transientDescriptorAllocator,
	CircularUploadMemoryAllocator& transientUploadMemoryAllocator)
{
	uint32 transientResourcePoolMemoryUsed = 0;

	for (uint32 i = 0; i < resourceCount; i++)
	{
		Resource& resource = resources[i];

		if (resource.lifetime != ResourceLifetime::Transient)
			continue;

		if (resource.type == ResourceType::Buffer)
		{
			const uint64 requiredMemorySize = alignUp<uint64>(resource.transientBufferSize, 65536);
			const uint64 memoryOffset = transientResourcePoolMemoryUsed;
			transientResourcePoolMemoryUsed += requiredMemorySize;

			const HAL::BufferHandle buffer =
				device->createBuffer(resource.transientBufferSize, true, transientResourcePool.getMemory(), memoryOffset);

			TransientResourceInfo resourceInfo = {};
			resourceInfo.bufferHandle = buffer;
			resourceInfo.isTexture = false;
			transientResources.pushBack(resourceInfo);
		}
		else if (resource.type == ResourceType::Texture)
		{
			const ResourceAllocationInfo textureAllocationInfo = device->getTextureAllocationInfo(resource.transientTextureDesc);

			const uint64 requiredMemorySize = alignUp<uint64>(textureAllocationInfo.size, 65536); // TODO: Do we need alignUp here?
			const uint64 memoryOffset = transientResourcePoolMemoryUsed;
			transientResourcePoolMemoryUsed += requiredMemorySize;

			const HAL::TextureHandle texture =
				device->createTexture(resource.transientTextureDesc, transientResourcePool.getMemory(), memoryOffset);

			TransientResourceInfo resourceInfo = {};
			resourceInfo.textureHandle = texture;
			resourceInfo.isTexture = true;
			transientResources.pushBack(resourceInfo);
		}
	}

	PassExecutionBroker broker;

	for (uint32 i = 0; i < passCount; i++)
	{
		Pass& pass = passes[i];

		pass.executporFunc(broker, *device, commandList, pass.userData);
	}

	for (TransientResourceInfo& resourceInfo : transientResources)
	{
		if (resourceInfo.isTexture)
			device->destroyTexture(resourceInfo.textureHandle);
		else
			device->destroyBuffer(resourceInfo.bufferHandle);
	}
}
