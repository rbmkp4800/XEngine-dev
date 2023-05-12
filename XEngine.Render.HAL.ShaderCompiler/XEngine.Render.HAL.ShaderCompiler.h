#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.Common.h>

namespace XEngine::Render::HAL::ShaderCompiler
{
	class Host;

	// TODO: This should be `class Blob : public XLib::RefCounted`.
	// User should manipulate with `BlobRef` which is `XLib::RefCountPtr<Blob>`.
	// Blob creation should be done via separate `BlobBuilder`.
	class Blob : public XLib::NonCopyable
	{
	private:
		struct Preamble
		{
			volatile uint32 referenceCount;
			uint32 dataSize;
			//BlobUIDHash uidHash; // Should be SHA1/SHA256.
		};

	private:
		Preamble* data = nullptr;

	public:
		Blob() = default;
		~Blob();

		inline Blob(Blob&& that) { moveFrom(that); }
		inline void operator = (Blob&& that) { moveFrom(that); }

		inline void destroy() { this->~Blob(); }
		inline void moveFrom(Blob& that) { destroy(); data = that.data; that.data = nullptr; }
		inline Blob addReference() const;

		inline const void* getData() const { XAssert(data); return data + 1; }
		inline uint32 getSize() const { XAssert(data); return data->dataSize; }

		inline bool isInitialized() const { return data != nullptr; }

	public:
		static void* Create(uint32 size, Blob& blob);
	};

	using BindingNameInplaceString = XLib::InplaceStringASCIIx64;
	static constexpr uint8 MaxBindingNameLength = BindingNameInplaceString::GetMaxLength();

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

	struct DescriptorSetNestedBindingDesc
	{
		XLib::StringViewASCII name;
		uint16 descriptorCount; // Not implemented. Should be 1 for now.
		DescriptorType descriptorType;
	};

	struct PipelineBindingDesc
	{
		XLib::StringViewASCII name;
		PipelineBindingType type;

		// TODO: shader visibility
		uint8 constantsSize;
		const Blob* compiledDescriptorSetLayout; // TODO: This probably should not be pointer...
	};

	struct Define
	{
		XLib::StringViewASCII name;
		XLib::StringViewASCII value;
	};

	struct GraphicsPipelineDesc
	{
		// TODO: These probably should not be pointers...
		const Blob* compiledVertexShader;
		const Blob* compiledAmplificationShader;
		const Blob* compiledMeshShader;
		const Blob* compiledPixelShader;
		TexelViewFormat renderTargetsFormats[MaxRenderTargetCount];
		DepthStencilFormat depthStencilFormat;
	};

	struct GraphicsPipelineCompilationResult
	{
		Blob baseBlob;
		Blob bytecodeBlobs[MaxGraphicsPipelineBytecodeBlobCount];
	};

	class Host abstract final
	{
	public:
		static bool CompileDescriptorSetLayout(Platform platform,
			const DescriptorSetNestedBindingDesc* bindings, uint16 bindingCount, Blob& resultBlob);

		static bool CompilePipelineLayout(Platform platform,
			const PipelineBindingDesc* bindings, uint16 bindingCount,
			Blob& resultBlob, Blob& resultMetadataBlob);

		//static bool CompileShaderLibrary(Platform platform,
		//	const CompiledPipelineLayout& pipelineLayout, const char* source, uint32 sourceLength,
		//	const Define* defines, uint16 defineCount, CompiledShaderLibrary& result);

		//static bool CompileShader(Platform platform,
		//	const CompiledPipelineLayout& pipelineLayout, const CompiledShaderLibrary& library,
		//	ShaderType shaderType, const char* entryPointName, CompiledShader& result);

		static bool CompileShader(Platform platform,
			const Blob& compiledPipelineLayoutBlob, const Blob& pipelineLayoutMetadataBlob,
			const char* source, uint32 sourceLength, ShaderType shaderType, const char* displayedShaderFilename,
			const char* entryPointName, Blob& resultBlob);

		static bool CompileGraphicsPipeline(Platform platform, const Blob& compiledPipelineLayout,
			const GraphicsPipelineDesc& desc, GraphicsPipelineCompilationResult& result);
	};
}
