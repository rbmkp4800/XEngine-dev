#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.Common.h>

namespace XEngine::Render::HAL::ObjectFormat { struct PipelineBindPointRecord; }

namespace XEngine::Render::HAL::ShaderCompiler
{
	class CompiledDescriptorBundleLayout;
	class CompiledPipelineLayout;
	class CompiledShaderLibrary;
	class CompiledShader;
	class CompiledGraphicsPipeline;
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
		XLib::StringViewASCII name;
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

	struct Define
	{
		const char* name;
		const char* value;
	};

	struct GraphicsPipelineDesc
	{
		// TODO: Input assempler?
		const CompiledShader* vertexShader;
		const CompiledShader* amplificationShader;
		const CompiledShader* meshShader;
		const CompiledShader* pixelShader;
		TexelViewFormat renderTargetsFormats[MaxRenderTargetCount];
		DepthStencilFormat depthStencilFormat;
	};

	struct ObjectHash
	{
		uint64 a;
		uint64 b;
	};

	inline bool operator == (const ObjectHash& left, const ObjectHash& right) { return left.a == right.a && left.b == right.b; }

	class Object : public XLib::NonCopyable
	{
		friend Host;

	private:
		struct alignas(32) BlockHeader
		{
			volatile uint32 referenceCount;
			uint32 dataSize;
			ObjectHash hash;
			bool finalized;
		};

	private:
		BlockHeader* block = nullptr;

	private:
		void fillGenericHeaderAndFinalize(uint64 signature);

	public:
		Object() = default;
		~Object();

		inline Object(Object&& that) { moveFrom(that); }
		inline void operator = (Object&& that) { moveFrom(that); }

		inline void release() { this->~Object(); }
		inline void moveFrom(Object& that) { release(); block = that.block; that.block = nullptr; }

		void finalize();
		Object clone() const;
		uint32 getCRC() const;

		inline void* getMutableData() { XAssert(block && !block->finalized); return block + 1; }
		inline const void* getData() const { XAssert(block && block->finalized); return block + 1; }
		inline uint32 getSize() const { XAssert(block); return block->dataSize; }
		inline ObjectHash getHash() const { XAssert(block && block->finalized); return block->hash; }

		inline bool isInitialized() const { return block != nullptr; }
		inline bool isValid() const { return block ? block->finalized : false; }

	public:
		static Object Create(uint32 size);
	};

	class CompiledDescriptorBundleLayout : public XLib::NonCopyable
	{
		friend Host;

	private:
		Object object;

	public:
		CompiledDescriptorBundleLayout() = default;
		~CompiledDescriptorBundleLayout() = default;

		inline void destroy() { this->~CompiledDescriptorBundleLayout(); }

		inline bool isInitialized() const { return object.isValid(); }
		inline const Object& getObject() const { return object; }
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
		Object object;
		BindPointMetadata bindPointsMetadata[MaxPipelineBindPointCount] = {};

	private:
		bool findBindPointMetadata(uint32 bindPointNameCRC, BindPointMetadata& result) const;
		uint32 getSourceHash() const;

	public:
		CompiledPipelineLayout() = default;
		~CompiledPipelineLayout() = default;

		inline void destroy() { this->~CompiledPipelineLayout(); }

		inline bool isInitialized() const { return object.isValid(); }
		inline const Object& getObject() const { return object; }
	};

	class CompiledShaderLibrary : public XLib::NonCopyable
	{
		friend Host;

	private:
		Object object;

	public:
		CompiledShaderLibrary() = default;
		~CompiledShaderLibrary() = default;
	};

	class CompiledShader : public XLib::NonCopyable
	{
		friend Host;

	private:
		Object object;

	public:
		CompiledShader() = default;
		~CompiledShader() = default;

		inline void destroy() { this->~CompiledShader(); }

		ShaderType getShaderType() const;

		inline bool isInitialized() const { return object.isValid(); }
		inline const Object& getObject() const { return object; }
	};

	class CompiledGraphicsPipeline : public XLib::NonCopyable
	{
		friend Host;

	private:
		Object baseObject;
		Object bytecodeObjects[MaxGraphicsPipelineBytecodeObjectCount];
		uint8 bytecodeObjectCount = 0;

	public:
		CompiledGraphicsPipeline() = default;
		~CompiledGraphicsPipeline() = default;

		inline void destroy() { this->~CompiledGraphicsPipeline(); }

		inline bool isInitialized() const { return bytecodeObjectCount > 0; }
		inline const Object& getBaseObject() const { return baseObject; }
		inline const Object& getBytecodeObject(uint8 bytecodeObjectIndex) const { return bytecodeObjects[bytecodeObjectIndex]; }
		inline uint8 getBytecodeObjectCount() const { return bytecodeObjectCount; }
	};

	class Host abstract final
	{
	public:
		static bool CompileDescriptorBundleLayout(Platform platform,
			const DescriptorBindPointDesc* bindPoints, uint8 bindPointCount,
			CompiledDescriptorBundleLayout& result);

		static bool CompilePipelineLayout(Platform platform,
			const PipelineBindPointDesc* bindPoints, uint8 bindPointCount, CompiledPipelineLayout& result);

		static bool CompileCompiledShaderLibrary(Platform platform,
			const CompiledPipelineLayout& pipelineLayout, const char* source, uint32 sourceLength,
			const Define* defines, uint16 defineCount, CompiledShaderLibrary& result);

		static bool CompileShader(Platform platform,
			const CompiledPipelineLayout& pipelineLayout, const CompiledShaderLibrary& library,
			ShaderType shaderType, const char* entryPointName, CompiledShader& result);

		// Temporary
		static bool CompileShader(Platform platform, const CompiledPipelineLayout& pipelineLayout,
			ShaderType shaderType, const char* shaderName, const char* source, uint32 sourceLength, CompiledShader& result);

		static bool CompileGraphicsPipeline(Platform platform, const CompiledPipelineLayout& pipelineLayout,
			const GraphicsPipelineDesc& desc, CompiledGraphicsPipeline& result);
	};
}
