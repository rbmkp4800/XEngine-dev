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

	enum class PipelineBindPointShaderVisibility : uint8
	{
		All = 0,
		Vertex,
		Amplification,
		Mesh,
		Pixel,
	};

	struct PipelineBindPointDesc
	{
		const char* name;
		PipelineBindPointType type;
		PipelineBindPointShaderVisibility shaderVisibility;
		union
		{
			uint8 constantsSize32bitValues;
			const CompiledDescriptorBundleLayout* descriptorBundleLayout;
		};
	};

	struct DescriptorBindPointDesc
	{
		const char* name;
		DescriptorType descriptorType;
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
			struct BlockHeader;

		private:
			BlockHeader* block = nullptr;

		public:
			SharedDataBufferRef() = default;
			inline ~SharedDataBufferRef() { release(); }

			SharedDataBufferRef createReference();
			void release();

			void* getMutablePointer();

			DataView getData() const;
			inline bool isValid() const { return block != nullptr; }

		public:
			static SharedDataBufferRef AllocateBuffer(uint32 size);
		};
	}

	class CompiledDescriptorBundleLayout : public XLib::NonCopyable
	{
		friend Host;

	private:
		Internal::SharedDataBufferRef objectData;

	public:
		CompiledDescriptorBundleLayout() = default;
		~CompiledDescriptorBundleLayout() = default;

		inline bool isInitialized() const { return objectData.isValid(); }
		inline DataView getObjectData() const { return objectData.getData(); }
	};

	class CompiledPipelineLayout : public XLib::NonCopyable
	{
		friend Host;

	private:
		Internal::SharedDataBufferRef objectData;

	public:
		CompiledPipelineLayout() = default;
		~CompiledPipelineLayout() = default;

		inline bool isInitialized() const { return objectData.isValid(); }
		inline DataView getObjectData() const;
	};

	class CompiledShader : public XLib::NonCopyable
	{
		friend Host;

	private:
		Internal::SharedDataBufferRef objectData;

	public:
		CompiledShader() = default;
		~CompiledShader() = default;

		inline bool isInitialized() const { return objectData.isValid(); }
		inline DataView getObjectData() const { return objectData.getData(); }
	};

	class CompiledPipeline : public XLib::NonCopyable
	{
		friend Host;

	private:
		Internal::SharedDataBufferRef baseObjectData;
		Internal::SharedDataBufferRef bytecodeObjectsData[4];

	public:
		CompiledPipeline() = default;
		~CompiledPipeline() = default;

		inline bool isInitialized() const { return baseObjectData.isValid(); }
		inline DataView getBaseObjectData() const { return baseObjectData.getData(); }
		inline DataView getBytecodeObjectData(uint8 bytecodeObjectIndex) const;
		inline uint8 getBytecodeObjectCount() const;
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
