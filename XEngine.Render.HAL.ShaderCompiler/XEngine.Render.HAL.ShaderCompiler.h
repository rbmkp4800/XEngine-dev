#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.Common.h>

// TODO: Use SHA1/SHA256 for ObjectLongHash. CRC64 is used for now.

namespace XEngine::Render::HAL::ShaderCompiler
{
	class CompiledDescriptorSetLayout;
	class CompiledPipelineLayout;
	class CompiledShaderLibrary;
	class CompiledShader;
	class CompiledGraphicsPipeline;
	class Host;

	enum class Platform : uint8
	{
		Undefined = 0,
		D3D12,
		Vulkan,
		//Scarlett,
		//Prospero,
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

	struct DescriptorSetBindingDesc
	{
		XLib::StringViewASCII name;
		DescriptorType descriptorType;
	};

	struct PipelineBindingDesc
	{
		XLib::StringViewASCII name;
		PipelineBindingType type;

		// TODO: shader visibility
		uint8 constantsSize;
		const CompiledDescriptorSetLayout* descriptorSetLayout;
	};

	struct Define
	{
		XLib::StringViewASCII name;
		XLib::StringViewASCII value;
	};

	struct GraphicsPipelineDesc
	{
		const CompiledShader* vertexShader;
		const CompiledShader* amplificationShader;
		const CompiledShader* meshShader;
		const CompiledShader* pixelShader;
		TexelViewFormat renderTargetsFormats[MaxRenderTargetCount];
		DepthStencilFormat depthStencilFormat;
	};

	using ObjectLongHash = uint64;

	class Object : public XLib::NonCopyable
	{
		friend Host;

	private:
		struct alignas(32) BlockHeader
		{
			volatile uint32 referenceCount;
			uint32 dataSize;
			ObjectLongHash longHash;
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
		uint32 getCRC32() const;

		inline void* getMutableData() { XAssert(block && !block->finalized); return block + 1; }
		inline const void* getData() const { XAssert(block && block->finalized); return block + 1; }
		inline uint32 getSize() const { XAssert(block); return block->dataSize; }
		inline ObjectLongHash getLongHash() const { XAssert(block && block->finalized); return block->longHash; }

		inline bool isInitialized() const { return block != nullptr; }
		inline bool isValid() const { return block ? block->finalized : false; }

	public:
		static Object Create(uint32 size);
	};

	class CompiledDescriptorSetLayout : public XLib::NonCopyable
	{
		friend Host;

	private:
		Object object;

	public:
		CompiledDescriptorSetLayout() = default;
		~CompiledDescriptorSetLayout() = default;

		inline void destroy() { this->~CompiledDescriptorSetLayout(); }

		inline bool isInitialized() const { return object.isValid(); }
		inline const Object& getObject() const { return object; }
	};

	class CompiledPipelineLayout : public XLib::NonCopyable
	{
		friend Host;

	private:
		struct BindingMetadata
		{
			uint8 registerIndex;
			char registerType; // b/t/u
		};

	private:
		Object object;
		BindingMetadata bindingsMetadata[MaxPipelineBindingCount] = {};

	private:
		bool findBindingMetadata(uint32 bindingNameXSH, BindingMetadata& result) const;
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
		static bool CompileDescriptorSetLayout(Platform platform,
			const DescriptorSetBindingDesc* bindings, uint8 bindingCount,
			CompiledDescriptorSetLayout& result);

		static bool CompilePipelineLayout(Platform platform,
			const PipelineBindingDesc* bindings, uint8 bindingCount, CompiledPipelineLayout& result);

		static bool CompileShaderLibrary(Platform platform,
			const CompiledPipelineLayout& pipelineLayout, const char* source, uint32 sourceLength,
			const Define* defines, uint16 defineCount, CompiledShaderLibrary& result);

		static bool CompileShader(Platform platform,
			const CompiledPipelineLayout& pipelineLayout, const CompiledShaderLibrary& library,
			ShaderType shaderType, const char* entryPointName, CompiledShader& result);

		// Temporary
		static bool CompileShader(Platform platform, const CompiledPipelineLayout& pipelineLayout,
			const char* source, uint32 sourceLength, ShaderType shaderType, const char* displayedShaderFilename,
			const char* entryPointName, CompiledShader& result);

		static bool CompileGraphicsPipeline(Platform platform, const CompiledPipelineLayout& pipelineLayout,
			const GraphicsPipelineDesc& desc, CompiledGraphicsPipeline& result);
	};
}
