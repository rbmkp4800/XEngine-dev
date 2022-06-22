#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.D3D12.h>

namespace XEngine::Render::Scheduler
{
	class Scheduler;
	class Pipeline;
	class Pass;

	using TransientTextureHandle = uint32;
	using TransientBufferHandle = uint32;

	using TransientTextureViewHandle = uint32;
	using TransientBufferViewHandle = uint32;
	using TransientRenderTargetViewHandle = uint32;
	using TransientDepthStencilViewHandle = uint32;

	using PassCPUExecutorFunc = void(*)();

	enum class TextureAccessMode : uint8
	{
		Undefined = 0,
		Read,
		Write,
		ReadWrite,
		RenderTarget,
		ReadOnlyDepthStencil,
		ReadWriteDepthStencil,
	};

	enum class BufferAccessMode : uint8
	{
		Undefined = 0,
		Read,
		Write,
		ReadWrite,
	};

	enum class PassDependencyType : uint8
	{
		Undefined = 0,
		TransientTexture,
		TransientBuffer,
		VirtualCPUResource,
		ExternalTexture,
		ExternalBuffer,
		ExternalGPUExecutionKickOffSignal,
		ExternalCPUExecutionKickOffSignal,
	};

	struct PassDependencyDesc
	{
		PassDependencyType type;
	};

	enum class PassGPUUsageType : uint8
	{
		None = 0,
		Graphics,
		Compute,
	};

	class Pipeline : public XLib::NonCopyable
	{
	public:
		Pipeline() = default;
		~Pipeline() = default;

		TransientTextureHandle createTransientTexture(HAL::TextureType type, uint16x3 size, HAL::TextureFormat format);
		TransientBufferHandle createTransientBuffer(uint64 size);

		TransientTextureViewHandle createTransientTextureView(TransientTextureHandle texture);
		TransientBufferViewHandle createTransientBufferView(TransientBufferHandle buffer);
		TransientRenderTargetViewHandle createTransientRenderTargetView(TransientTextureHandle texture);
		TransientDepthStencilViewHandle createTransientDepthStencilView(TransientTextureHandle texture);

		void createVirtualCPUResource();

		void registerExternalTexture(HAL::ResourceHandle texture);
		void registerExternalBuffer(HAL::ResourceHandle buffer);

		void createPass(PassGPUUsageType gpuUsageType, const PassDependencyDesc* dependencies, uint32 dependecyCount,
			PassCPUExecutorFunc cpuExecutorFunc, const void* cpuExecutorContext, uint32 cpuExecutorContextSize);

		void compileDependenciesGraph();

		uint64 getTransientMemoryRequirement();
	};

	class Scheduler : public XLib::NonCopyable
	{
	private:

	public:
		Scheduler() = default;
		~Scheduler() = default;

		void submit(Pipeline& pipeline, HAL::MemoryBlockHandle transientMemoryBlock, uint64 transientMemoryOffset);
	};
}
