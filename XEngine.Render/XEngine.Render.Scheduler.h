#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.D3D12.h>

// NOTE: This is dummy "placeholder" implementation just to get interface working.

namespace XEngine::Render::Scheduler
{
	enum class BufferHandle : uint32 {};
	enum class TextureHandle : uint32 {};

	struct TransientMemoryAllocationInfo
	{
		HAL::BufferHandle buffer;
		uint32 offset;
	};

	class PassExecutionBroker : public XLib::NonCopyable
	{
		friend class Pipeline;

	private:

	private:
		PassExecutionBroker() = default;
		~PassExecutionBroker() = default;

	public:
		HAL::DescriptorSetReference allocateTransientDescriptorSet(HAL::DescriptorSetLayoutHandle descriptorSetLayout);
		HAL::DescriptorAddress allocateTransientDescriptors(uint16 descriptorCount);
		TransientMemoryAllocationInfo allocateTransientMemory(uint32 size, uint32 alignment = 0);

		HAL::ResourceViewHandle getTransientBufferView(BufferHandle buffer);
		HAL::ResourceViewHandle getTransientTextureView(TextureHandle texture);
		HAL::RenderTargetViewHandle getTransientRenderTargetView(TextureHandle texture);
		HAL::DepthStencilViewHandle getTransientDepthStencilView(TextureHandle texture);
	};

	using PassExecutorFunc = void(*)(PassExecutionBroker& executionBroker,
		HAL::Device& device, HAL::CommandList& commandList, const void* userData);

	enum class BufferAccess : uint8
	{
		Undefined = 0,
		ShaderRead,
		ShaderWrite,
		ShaderReadWrite,
	};

	enum class TextureAccess : uint8
	{
		Undefined = 0,
		ShaderRead,
		ShaderWrite,
		ShaderReadWrite,
		RenderTarget,
		DepthStencilReadOnly,
		DepthStencilReadWrite,
	};

	enum class PassDependencyType : uint8
	{
		Undefined = 0,
		Buffer,
		Texture,
		//ExternalCPUExecutionKickOffSignal,
		//ExternalGPUExecutionKickOffSignal,
		//VirtualCPUResource,
	};

#if 0

	struct PassDependencyDesc
	{
		PassDependencyType type;

		union
		{
			struct
			{
				PipelineBufferHandle buffer;
				BufferAccess access;
			} buffer;

			struct
			{
				PipelineTextureHandle texture;
				TextureAccess access;
			} texture;
		};
	};

#else

	struct PassBufferDependencyDesc
	{
		BufferHandle buffer;
		BufferAccess access;
	};

	struct PassTextureDependencyDesc
	{
		TextureHandle texture;
		TextureAccess access;
	};

	struct PassDependenciesDesc
	{
		const PassBufferDependencyDesc* bufferDependencies;
		const PassTextureDependencyDesc* textureDependencies;
		uint8 bufferDependencyCount;
		uint8 textureDependencyCount;
	};

#endif

	enum class PassType : uint8
	{
		None = 0,
		Graphics,
		Compute,
		Copy,
	};

	class TransientResourcePool : public XLib::NonCopyable
	{
	private:
		struct Entry;

	private:
		HAL::MemoryBlockHandle memoryBlock = HAL::MemoryBlockHandle::Zero;
		uint64 memoryBlockSize = 0;

		Entry* entries = nullptr;

		HAL::DeviceQueueSyncPoint releaseSyncPoint = HAL::DeviceQueueSyncPoint::Zero;

	public:
		TransientResourcePool() = default;
		~TransientResourcePool() = default;

		void initialize(HAL::Device& device, HAL::MemoryBlockHandle memoryBlock, uint64 memoryBlockSize);
		void reset();

		inline HAL::MemoryBlockHandle getMemory() const { return memoryBlock; }
	};

	/*class PipelineCommandListPool : public XLib::NonCopyable
	{

	};*/

	class TransientMemoryCircularAllocator : public XLib::NonCopyable
	{

	};

	class TransientDescriptorCircularAllocator : public XLib::NonCopyable
	{

	};

	class Pipeline : public XLib::NonCopyable
	{
	private:
		enum class ResourceType : uint8;

		struct Resource;
		struct Pass;

	private:
		HAL::CommandList commandList;

		Resource* resources = nullptr;
		Pass* passes = nullptr;

		uint32 resourceCount = 0;
		uint32 passCount = 0;

	public:
		Pipeline() = default;
		~Pipeline() = default;

		//void open();
		//void close();

		BufferHandle createTransientBuffer(uint64 size);
		TextureHandle createTransientTexture(const HAL::TextureDesc& textureDesc);

		BufferHandle importExternalBuffer(HAL::BufferHandle buffer);
		TextureHandle importExternalTexture(HAL::TextureHandle texture);

		void* allocateUserData(uint32 size);

		//void createVirtualCPUResource();

		void schedulePass(PassType type, const PassDependenciesDesc& dependencies,
			PassExecutorFunc executorFunc, const void* userData);

		void compile();

		uint64 getTransientResourcePoolMemoryRequirement() const;

		void executeAndSubmitToDevice(HAL::Device& device,
			TransientResourcePool& transientResourcePool,
			TransientDescriptorCircularAllocator& transientDescriptorAllocator,
			TransientMemoryCircularAllocator& transientMemoryAllocator);
	};
}
