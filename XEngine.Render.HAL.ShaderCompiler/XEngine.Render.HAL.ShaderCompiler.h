#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.Common.h>

namespace XEngine::Render::HAL::ShaderCompiler
{
	class CompiledDescriptorBundleLayout;
	class CompiledPipelineLayout;
	class CompiledShader;
	class CompiledPipeline;
	class Host;

	enum class Platform : uint8
	{
		Undefined = 0,
		D3D12,
		//D3D12X,
		//AGC,
		//Vulkan,
	};

	enum class ShaderType : uint8
	{
		Undefined = 0,
		Compute,
		Vertex,
		Amplification,
		Mesh,
		Pixel,
	};

	enum class PipelineBindPointType : uint8
	{
		Undefined = 0,
		Constants,
		ConstantBuffer,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		//Descriptor,
		//DescriptorBundle,
		//DescriptorArray,
	};

	enum class DescriptorBindPointType : uint8
	{
		Undefined = 0,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		ReadOnlyTexture2D,
		ReadWriteTexture2D,
		RaytracingAccelerationStructure,
	};

	struct PipelineBindPointDesc
	{
		const char* name;
		PipelineBindPointType type;
		uint8 shaderVisibility;
		uint8 constantCount;
		const CompiledDescriptorBundleLayout* descriptorBundleLayout;
	};

	struct DescriptorBindPointDesc
	{
		const char* name;
		DescriptorBindPointType type;
	};

	struct PipelineLayoutDesc
	{
		const PipelineBindPointDesc* bindPoints;
		uint8 bindPointCount;
	};

	struct DescriptorBundleLayoutDesc
	{
		const DescriptorBindPointDesc* bindPoints;
		uint8 bindPointCount;
	};

	struct GraphicsPipelineDesc
	{
		// TODO: Input assempler?
		CompiledShader* vertexShader;
		CompiledShader* amplificationShader;
		CompiledShader* meshShader;
		CompiledShader* pixelShader;
		TexelFormat renderTargetsFormats[4];
		TexelFormat depthStencilFormat;
	};
	
	struct ComputePipelineDesc
	{
		CompiledShader* computeShader;
	};

	struct DataView
	{
		const void* data;
		uint32 size;
	};

	namespace Internal
	{
		class SharedDataBufferRef : public XLib::NonCopyable
		{
		private:
			struct Header
			{
				uint32 referenceCount;
				uint32 dataSize;
			};

		private:
			Header* block = nullptr;

		public:
			SharedDataBufferRef() = default;
			inline ~SharedDataBufferRef() { release(); }

			void allocate(const DataView* segments, uint32 segmentCount);

			SharedDataBufferRef addReference();
			void release();

			inline bool isValid() const { return block != nullptr; }
		};
	}

	class CompiledDescriptorBundleLayout : public XLib::NonCopyable
	{
		friend Host;

	private:
		Internal::SharedDataBufferRef dataBuffer;

	public:
		CompiledDescriptorBundleLayout() = default;
		~CompiledDescriptorBundleLayout() = default;

		inline bool isInitialized() const;
		inline DataView getData() const;
	};

	class CompiledPipelineLayout : public XLib::NonCopyable
	{
		friend Host;

	private:
		Internal::SharedDataBufferRef dataBuffer;

	public:
		CompiledPipelineLayout() = default;
		~CompiledPipelineLayout() = default;

		inline bool isInitialized() const;
		inline DataView getData() const;

		inline uint32 getBindPointId(uint8 bindPointIndex) const;
	};

	class CompiledShader : public XLib::NonCopyable
	{
		friend Host;

	private:
		Internal::SharedDataBufferRef dataBuffer;

	public:
		CompiledShader() = default;
		~CompiledShader() = default;

		inline bool isInitialized() const;
		inline DataView getData() const;
	};

	class CompiledPipeline : public XLib::NonCopyable
	{
		friend Host;

	private:
		Internal::SharedDataBufferRef metadataBuffer;
		Internal::SharedDataBufferRef bytecodeChunksDataBuffers[4];

	public:
		CompiledPipeline() = default;
		~CompiledPipeline() = default;

		inline bool isInitialized() const;
		inline DataView getMetadata() const;
		inline DataView getBytecodeChunkData(uint8 bytecodeChunkIndex) const;
		inline uint8 getBytecodeChunkCount() const;
	};

	class Host abstract final
	{
	public:
		static bool CompileDescriptorBundleLayout(Platform platform,
			DescriptorBundleLayoutDesc& desc, CompiledDescriptorBundleLayout& result);

		static bool CompilePipelineLayout(Platform platform,
			const PipelineLayoutDesc& desc, CompiledPipelineLayout& result);

		static bool CompileShader(Platform platform, const CompiledPipelineLayout& compiledPipelineLayout,
			ShaderType shaderType, const char* source, uint32 sourceLength, CompiledShader& result);

		static bool CompileGraphicsPipeline(Platform platform, const CompiledPipelineLayout& compiledPipelineLayout,
			const GraphicsPipelineDesc& pipelineDesc, CompiledPipeline& result);

		static bool CompileComputePipeline(Platform platform, const CompiledPipelineLayout& compiledPipelineLayout,
			const ComputePipelineDesc& pipelineDesc, CompiledPipeline& result);
	};
}
