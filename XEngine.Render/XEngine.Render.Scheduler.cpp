#include "XEngine.Render.Scheduler.h"

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
		BufferHandle externalBufferHandle;
		TextureHandle externalTextureHandle;
		uint64 transientBufferSize;
		TextureDesc transientTextureDesc;
	};
};

struct Pipeline::Pass
{
	PassExecutorFunc executporFunc;
	const void* userData;
};

void Pipeline::open()
{

}

PipelineBufferHandle Pipeline::createTransientBuffer(uint64 size)
{

}

PipelineTextureHandle Pipeline::createTransientTexture(const TextureDesc& textureDesc)
{

}

void Pipeline::schedule(Device& device,
	TransientResourcePool& transientResourcePool,
	TransientDescriptorCircularAllocator& transientDescriptorAllocator,
	TransientMemoryCircularAllocator& transientMemoryAllocator)
{
	transientResourcePool.reset();

	for (uint32 i = 0; i < resourceCount; i++)
	{
		Resource& resource = resources[i];

		if (resource.type == ResourceType::TransientBuffer)
		{
			device.createBuffer(resource.transientBufferSize, );
		}
		else if (resource.type == ResourceType::TransientTexture)
		{
			device.createTexture(resource.transientTextureDesc, );
		}
	}

	PipelineSchedulerBroker broker;

	for (uint32 i = 0; i < passCount; i++)
	{
		Pass& pass = passes[i];

		pass.executporFunc(broker, device, commandList, pass.userData);
	}
}
