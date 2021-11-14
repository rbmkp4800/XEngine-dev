#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.HAL.Common.h>

namespace XEngine::Render::HAL::ObjectFormat { struct PipelineBindPointRecord; }

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
		const CompiledShader* vertexShader;
		const CompiledShader* amplificationShader;
		const CompiledShader* meshShader;
		const CompiledShader* pixelShader;
		TexelFormat renderTargetsFormats[MaxRenderTargetCount];
		TexelFormat depthStencilFormat;
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

			inline SharedDataBufferRef(SharedDataBufferRef&& that) { moveFrom(that); }
			inline void operator = (SharedDataBufferRef&& that) { moveFrom(that); }

			inline void moveFrom(SharedDataBufferRef& that) { release(); block = that.block; that.block = nullptr; }

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

		inline void destroy() { this->~CompiledDescriptorBundleLayout(); }

		inline bool isInitialized() const { return objectData.isValid(); }
		inline DataView getObjectData() const { return objectData.getData(); }
	};

	class CompiledPipelineLayout : public XLib::NonCopyable
	{
		friend Host;

	private:
		struct BindPointMetadata
		{
			uint8 registerIndex;
			char registerType; // b/t/u
		};

	private:
		Internal::SharedDataBufferRef objectData;
		BindPointMetadata bindPointsMetadata[MaxPipelineBindPointCount] = {};

	private:
		bool findBindPointMetadata(uint32 bindPointNameCRC, BindPointMetadata& result) const;
		uint32 getSourceHash() const;

	public:
		CompiledPipelineLayout() = default;
		~CompiledPipelineLayout() = default;

		inline void destroy() { this->~CompiledPipelineLayout(); }

		inline bool isInitialized() const { return objectData.isValid(); }
		inline DataView getObjectData() const;
	};

	class CompiledShader : public XLib::NonCopyable
	{
		friend Host;

	private:
		Internal::SharedDataBufferRef objectData;

	private:
		uint32 getObjectCRC() const;

	public:
		CompiledShader() = default;
		~CompiledShader() = default;

		inline void destroy() { this->~CompiledShader(); }

		ShaderType getShaderType() const;

		inline bool isInitialized() const { return objectData.isValid(); }
		inline DataView getObjectData() const { return objectData.getData(); }
	};

	class CompiledPipeline : public XLib::NonCopyable
	{
		friend Host;

	private:
		Internal::SharedDataBufferRef graphicsBaseObjectData;
		Internal::SharedDataBufferRef bytecodeObjectsData[MaxGraphicsPipelineBytecodeObjectCount];
		uint8 bytecodeObjectCount = 0;

	public:
		CompiledPipeline() = default;
		~CompiledPipeline() = default;

		inline void destroy() { this->~CompiledPipeline(); }

		inline bool isInitialized() const { return bytecodeObjectCount > 0; }
		inline DataView getGraphicsBaseObjectData() const { return graphicsBaseObjectData.getData(); }
		inline DataView getBytecodeObjectData(uint8 bytecodeObjectIndex) const { return bytecodeObjectsData[bytecodeObjectIndex].getData(); }
		inline uint8 getBytecodeObjectCount() const { return bytecodeObjectCount; }
	};

	class Host abstract final
	{
	public:
		static bool CompileDescriptorBundleLayout(Platform platform,
			DescriptorBundleLayoutDesc& desc, CompiledDescriptorBundleLayout& result);

		static bool CompilePipelineLayout(Platform platform,
			const PipelineLayoutDesc& desc, CompiledPipelineLayout& result);

		static bool CompileShader(Platform platform, const CompiledPipelineLayout& pipelineLayout,
			ShaderType shaderType, const char* source, uint32 sourceLength, CompiledShader& result);

		static bool CompileGraphicsPipeline(Platform platform, const CompiledPipelineLayout& pipelineLayout,
			const GraphicsPipelineDesc& desc, CompiledPipeline& result);

		static bool CompileComputePipeline(Platform platform, const CompiledPipelineLayout& pipelineLayout,
			const CompiledShader* computeShader, CompiledPipeline& result);
	};
}
