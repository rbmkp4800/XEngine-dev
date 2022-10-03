#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.D3D12.h>

namespace XEngine::Render::Scheduler
{
	class Scheduler;
	class Pipeline;
	class Pass;

	enum class TransientBufferHandle			: uint32 {};
	enum class TransientTextureHandle			: uint32 {};
	enum class TransientBufferViewHandle		: uint32 {};
	enum class TransientTextureViewHandle		: uint32 {};
	enum class TransientRenderTargetViewHandle	: uint32 {};
	enum class TransientDepthStencilViewHandle	: uint32 {};

	using PassCPUExecutorFunc = void(*)(HAL::CommandList& commandList, const void* context, uint32 contextSize);

	enum class BufferAccessMode : uint8
	{
		Undefined = 0,
		ShaderRead,
		ShaderWrite,
		ShaderReadWrite,
	};

	enum class TextureAccessMode : uint8
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
		TransientBuffer,
		TransientTexture,
		ExternalBuffer,
		ExternalTexture,
		ExternalCPUExecutionKickOffSignal,
		ExternalGPUExecutionKickOffSignal,
		//VirtualCPUResource,
	};

	struct PassDependencyDesc
	{
		PassDependencyType type;
	};

	enum class PassDeviceWorkloadType : uint8
	{
		None = 0,
		Graphics,
		Compute,
		Copy,
	};

	class Pipeline : public XLib::NonCopyable
	{
	public:
		Pipeline() = default;
		~Pipeline() = default;

		TransientBufferHandle createTransientBuffer(uint64 size);
		TransientTextureHandle createTransientTexture(HAL::TextureType type, uint16x3 size, HAL::TextureFormat format);

		TransientBufferViewHandle createTransientBufferView(TransientBufferHandle buffer);
		TransientTextureViewHandle createTransientTextureView(TransientTextureHandle texture);
		TransientRenderTargetViewHandle createTransientRenderTargetView(TransientTextureHandle texture);
		TransientDepthStencilViewHandle createTransientDepthStencilView(TransientTextureHandle texture);

		void createVirtualCPUResource();

		void registerExternalBuffer(HAL::ResourceHandle buffer);
		void registerExternalTexture(HAL::ResourceHandle texture);

		void addPass(PassDeviceWorkloadType deviceWorkloadType, const PassDependencyDesc* dependencies, uint32 dependecyCount,
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

		void submit(Pipeline& pipeline, HAL::MemoryBlockHandle transientMemory, uint64 transientMemoryOffset, uint64 transientMemorySize);
	};
}
