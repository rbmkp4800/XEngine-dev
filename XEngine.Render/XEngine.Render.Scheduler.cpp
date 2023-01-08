#include <XLib.Containers.ArrayList.h> // TODO: Remove

#include "XEngine.Render.Scheduler.h"

using namespace XLib; // TODO: Remove
using namespace XEngine::Render;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::Scheduler;

enum class Pipeline::ResourceType : uint8
{
	Undefined = 0,
	TransientBuffer,
	TransientTexture,
	ExternalBuffer,
	ExternalTexture,
};

struct Pipeline::Resource
{
	ResourceType type;

	union
	{
		uint64 transientBufferSize;
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

Scheduler::BufferHandle Pipeline::createTransientBuffer(uint64 size)
{

}

Scheduler::TextureHandle Pipeline::createTransientTexture(const TextureDesc& textureDesc)
{

}

void Pipeline::executeAndSubmitToDevice(Device& device,
	TransientResourcePool& transientResourcePool,
	TransientDescriptorCircularAllocator& transientDescriptorAllocator,
	TransientMemoryCircularAllocator& transientMemoryAllocator)
{
	struct TransientResourceInfo
	{
		union
		{
			HAL::BufferHandle bufferHandle;
			HAL::TextureHandle textureHandle;
		};
		bool isTexture;
	};

	ArrayList<TransientResourceInfo> transientResources;

	uint32 transientResourcePoolMemoryUsed = 0;

	for (uint32 i = 0; i < resourceCount; i++)
	{
		Resource& resource = resources[i];

		if (resource.type == ResourceType::TransientBuffer)
		{
			const uint64 requiredMemorySize = alignUp<uint64>(resource.transientBufferSize, 65536);
			const uint64 memoryOffset = transientResourcePoolMemoryUsed;
			transientResourcePoolMemoryUsed += requiredMemorySize;

			const HAL::BufferHandle buffer =
				device.createBuffer(resource.transientBufferSize, true, transientResourcePool.getMemory(), memoryOffset);

			TransientResourceInfo resourceInfo = {};
			resourceInfo.bufferHandle = buffer;
			resourceInfo.isTexture = false;
			transientResources.pushBack(resourceInfo);
		}
		else if (resource.type == ResourceType::TransientTexture)
		{
			const ResourceAllocationInfo textureAllocationInfo = device.getTextureAllocationInfo(resource.transientTextureDesc);

			const uint64 requiredMemorySize = alignUp<uint64>(textureAllocationInfo.size, 65536); // TODO: Do we need alignUp here?
			const uint64 memoryOffset = transientResourcePoolMemoryUsed;
			transientResourcePoolMemoryUsed += requiredMemorySize;

			const HAL::TextureHandle texture =
				device.createTexture(resource.transientTextureDesc, transientResourcePool.getMemory(), memoryOffset);

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

		pass.executporFunc(broker, device, commandList, pass.userData);
	}

	for (TransientResourceInfo& resourceInfo : transientResources)
	{
		if (resourceInfo.isTexture)
			device.destroyTexture(resourceInfo.textureHandle);
		else
			device.destroyBuffer(resourceInfo.bufferHandle);
	}
}
