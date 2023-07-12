#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.RefCounted.h>
#include <XLib.String.h>

#include <XEngine.GfxHAL.Common.h>

// TODO: Pipeline bindings shader visibility.

namespace XEngine::GfxHAL::ShaderCompiler
{
	constexpr uint16 MaxPipelineBindingNameLength = 254;
	constexpr uint16 MaxDescriptorSetBindingNameLength = 254;

	constexpr uint32 StartShaderRegiserIndex = 1000;

	class DescriptorSetLayout;
	class PipelineLayout;
	class Blob;
	class ShaderCompilationArtifacts;

	using DescriptorSetLayoutRef = XLib::RefCountedPtr<DescriptorSetLayout>;
	using PipelineLayoutRef = XLib::RefCountedPtr<PipelineLayout>;
	using BlobRef = XLib::RefCountedPtr<Blob>;
	using ShaderCompilationArtifactsRef = XLib::RefCountedPtr<ShaderCompilationArtifacts>;

	enum class Platform : uint8
	{
		Undefined = 0,
		D3D12,
		Vulkan,
		//Scarlett,
		//Prospero,
	};

	struct DescriptorSetBindingDesc
	{
		XLib::StringViewASCII name;
		uint16 descriptorCount; // Not implemented. Should be 1 for now.
		GfxHAL::DescriptorType descriptorType;
	};

	struct PipelineBindingDesc
	{
		XLib::StringViewASCII name;
		GfxHAL::PipelineBindingType type;

		union
		{
			const DescriptorSetLayout* descriptorSetLayout;
			uint8 inplaceConstantCount;
		};
	};

	enum class VertexBufferType : uint8
	{
		Undefined = 0,
		PerVertex,
		PerInstance,
	};

	struct VertexBufferDesc
	{
		VertexBufferType type;
	};

	struct VertexBindingDesc
	{
		XLib::StringViewASCII name;
		uint16 offset;
		GfxHAL::TexelViewFormat format;
		uint8 bufferIndex;
	};

	struct ShaderDesc
	{
		XLib::StringViewASCII sourcePath;
		XLib::StringViewASCII sourceText;
		XLib::StringViewASCII entryPointName;
		// Optimization, platform specific stuff, etc.
	};

	struct GraphicsPipelineShaders
	{
		union
		{
			struct
			{
				ShaderDesc vs;
				ShaderDesc as;
				ShaderDesc ms;
				ShaderDesc ps;
			};
			struct
			{
				ShaderDesc all[4];
			};
		};
	};

	struct GraphicsPipelineSettings
	{
		VertexBufferDesc vertexBuffers[GfxHAL::MaxVertexBufferCount];
		VertexBindingDesc* vertexBindings;
		uint8 vertexBindingCount;

		GfxHAL::TexelViewFormat renderTargetsFormats[GfxHAL::MaxRenderTargetCount];
		GfxHAL::DepthStencilFormat depthStencilFormat;
	};

	class DescriptorSetLayout final : public XLib::RefCounted
	{
	private:
		struct BindingDesc
		{
			uint16 nameOffset;
			uint16 nameLength;
			uint32 descriptorOffset;
			uint16 descriptorCount;
			GfxHAL::DescriptorType descriptorType;
		};

	private:
		const BindingDesc* bindings = nullptr;
		XLib::StringViewASCII namesBuffer;
		const void* serializedBlobData = nullptr;
		uint32 serializedBlobSize = 0;
		uint32 sourceHash = 0;
		uint32 descriptorCount = 0;
		uint16 bindingCount = 0;

	private:
		DescriptorSetLayout() = default;
		virtual ~DescriptorSetLayout() override = default;

	public:
		inline uint16 getBindingCount() const { return bindingCount; }
		inline uint32 getDescriptorCount() const { return descriptorCount; }
		inline uint32 getSourceHash() const { return sourceHash; }

		sint16 findBinding(XLib::StringViewASCII name) const; // Returns negative number on failure.
		DescriptorSetBindingDesc getBindingDesc(uint16 bindingIndex) const;
		uint32 getBindingDescriptorOffset(uint16 bindingIndex) const;

		inline const void* getSerializedBlobData() const { return serializedBlobData; }
		inline uint32 getSerializedBlobSize() const { return serializedBlobSize; }
		uint32 getSerializedBlobChecksum() const; // Defined by 'GfxHAL::BlobFormat'

	public:
		static DescriptorSetLayoutRef Create(const DescriptorSetBindingDesc* bindings, uint16 bindingCount);
	};

	class PipelineLayout final : public XLib::RefCounted
	{
	private:
		struct BindingDesc
		{
			uint16 nameOffset;
			uint16 nameLength;
			uint32 baseShaderRegister;
			GfxHAL::PipelineBindingType type;
		};

	private:
		// TODO: Replace with something like `XLib::Array` that supports external storage and calls destructors.
		DescriptorSetLayoutRef descriptorSetLayoutTable[GfxHAL::MaxPipelineBindingCount];

		const BindingDesc* bindings = nullptr;
		XLib::StringViewASCII namesBuffer;
		const void* serializedBlobData = nullptr;
		uint32 serializedBlobSize = 0;
		uint32 sourceHash = 0;
		uint16 bindingCount = 0;

	private:
		PipelineLayout() = default;
		virtual ~PipelineLayout() override = default;

	public:
		inline uint16 getBindingCount() const { return bindingCount; }
		inline uint32 getSourceHash() const { return sourceHash; }

		sint16 findBinding(XLib::StringViewASCII name) const; // Returns negative number on failure.
		PipelineBindingDesc getBindingDesc(uint16 bindingIndex) const;
		uint32 getBindingBaseShaderRegister(uint16 bindingIndex) const;

		inline const void* getSerializedBlobData() const { return serializedBlobData; }
		inline uint32 getSerializedBlobSize() const { return serializedBlobSize; }
		uint32 getSerializedBlobChecksum() const; // Defined by 'GfxHAL::BlobFormat'

	public:
		static PipelineLayoutRef Create(const PipelineBindingDesc* bindings, uint16 bindingCount);
	};

	class Blob : public XLib::RefCounted
	{
	private:
		uint32 size;

	private:
		Blob() = default;
		virtual ~Blob() override = default;

	public:
		inline const void* getData() const { return this + 1; }
		inline uint32 getSize() const { return size; }
		uint32 getChecksum() const; // Defined by 'GfxHAL::BlobFormat'

	public:
		static BlobRef Create(uint32 size);
	};

	enum class ArtifactType : uint8
	{
		Undefined = 0,

		D3D_CompilerArgs,
		PreprocessedSource,
		Object,
		Disassembly,
		D3D_ShaderHash,
		D3D_PDB,
		//ExportedCBLayouts,

		EnumCount,
	};

	class ShaderCompilationArtifacts : public XLib::RefCounted
	{
	private:

	public:
		ShaderCompilationArtifacts() = default;
		~ShaderCompilationArtifacts() = default;

		void* getArtifactDataPtr(ArtifactType type) const;
		uint32 getArtifactSize(ArtifactType type) const;
		XLib::StringViewASCII getOutput() const;
	};

	struct GraphicsPipelineCompilationArtifacts
	{
		ShaderCompilationArtifactsRef vs;
		ShaderCompilationArtifactsRef as;
		ShaderCompilationArtifactsRef ms;
		ShaderCompilationArtifactsRef ps;
		XLib::DynamicStringASCII pipelineCompilationOutput;
	};

	struct GraphicsPipelineCompiledBlobs
	{
		BlobRef stateBlob;
		BlobRef vsBytecodeBlob;
		BlobRef asBytecodeBlob;
		BlobRef msBytecodeBlob;
		BlobRef psBytecodeBlob;
	};

	bool CompileGraphicsPipeline(
		const PipelineLayout& pipelineLayout,
		const GraphicsPipelineShaders& shaders,
		const GraphicsPipelineSettings& settings,
		GraphicsPipelineCompilationArtifacts& artifacts,
		GraphicsPipelineCompiledBlobs& compiledBlobs);

	bool CompileComputePipeline(
		const PipelineLayout& pipelineLayout,
		const ShaderDesc& computeShader,
		ShaderCompilationArtifactsRef& artifacts,
		BlobRef& compiledBlob);

	// For XDBS we can have something like this:
	//		SerializeGraphicsPipelineCompilationTask
	//		SerializeComputePipelineCompilationTask
	//		CompileSerializedTask
	//		DeserializeGraphicsPipelineCompilationResult
	//		DeserializeComputePipelineCompilationResult
}
