#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.RefCounted.h>
#include <XLib.String.h>

#include <XEngine.Gfx.HAL.Shared.h>

// TODO: Store used vertex attributes info in vertex shader blob (to validate in runtime).
// TODO: Store color RTs info in pixel shader blob (to validate in runtime).
// TODO: Pipeline bindings shader visibility.
// TODO: `getSourceHash` -> `getHash`.
// TODO: Remove `D3D12SerializeVersionedRootSignature` and all D3D12Core references.
// TODO: Pass include directories list as an argument to `CompileShader()`.

namespace XEngine::Gfx::HAL::ShaderCompiler
{
	constexpr uint16 MaxPipelineStaticSamplerCount = 8;
	constexpr uint16 MaxPipelineBindingNameLength = 125;
	constexpr uint16 MaxDescriptorSetBindingNameLength = 125;

	constexpr uint32 StartShaderRegiserIndex = 1000;

	class DescriptorSetLayout;
	class PipelineLayout;
	class Blob;
	class ShaderCompilationResult;

	using DescriptorSetLayoutRef = XLib::RefCountedPtr<DescriptorSetLayout>;
	using PipelineLayoutRef = XLib::RefCountedPtr<PipelineLayout>;
	using BlobRef = XLib::RefCountedPtr<Blob>;
	using ShaderCompilationResultRef = XLib::RefCountedPtr<ShaderCompilationResult>;

	struct GenericErrorMessage
	{
		XLib::InplaceStringASCIIx256 text;
	};

	struct StaticSamplerDesc
	{
		XLib::StringViewASCII bindingName;
		SamplerDesc desc;
	};

	struct DescriptorSetBindingDesc
	{
		XLib::StringViewASCII name;
		uint16 descriptorCount; // Not implemented. Should be 1 for now.
		DescriptorType descriptorType;
	};

	struct PipelineBindingDesc
	{
		XLib::StringViewASCII name;
		PipelineBindingType type;

		union
		{
			const DescriptorSetLayout* descriptorSetLayout;
			uint8 inplaceConstantCount;
		};
	};

	struct ShaderCompilationArgs
	{
		XLib::StringViewASCII entryPointName;
		// Optimization, platform specific stuff, etc.

		ShaderType shaderType;
	};

	class DescriptorSetLayout final : public XLib::RefCounted
	{
	private:
		struct InternalBindingDesc;

	private:
		const InternalBindingDesc* bindings = nullptr;
		XLib::StringViewASCII namesBuffer;
		const void* blobData = nullptr;
		uint32 blobSize = 0;
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

		inline const void* getBlobData() const { return blobData; }
		inline uint32 getBlobSize() const { return blobSize; }

	public:
		static DescriptorSetLayoutRef Create(const DescriptorSetBindingDesc* bindings, uint16 bindingCount,
			GenericErrorMessage& errorMessage);
	};

	class PipelineLayout final : public XLib::RefCounted
	{
	private:
		struct InternalBindingDesc;
		struct InternalStaticSamplerDesc;

	private:
		// TODO: Replace with something like 'XLib::InplaceArrayList' / 'XLib::Array' that supports external storage and calls destructors.
		DescriptorSetLayoutRef descriptorSetLayoutTable[MaxPipelineBindingCount];

		const InternalBindingDesc* bindings = nullptr;
		const InternalStaticSamplerDesc* staticSamplers = nullptr;
		XLib::StringViewASCII namesBuffer;
		const void* blobData = nullptr;
		uint32 blobSize = 0;
		uint32 sourceHash = 0;
		uint16 bindingCount = 0;
		uint16 staticSamplerCount = 0;

	private:
		PipelineLayout() = default;
		virtual ~PipelineLayout() override = default;

	public:
		inline uint16 getBindingCount() const { return bindingCount; }
		inline uint32 getSourceHash() const { return sourceHash; }

		sint16 findBinding(XLib::StringViewASCII name) const; // Returns negative number on failure.
		sint16 findStaticSampler(XLib::StringViewASCII bindingName) const; // Returns negative number on failure.
		PipelineBindingDesc getBindingDesc(uint16 bindingIndex) const;
		uint32 getBindingBaseShaderRegister(uint16 bindingIndex) const;
		uint32 getStaticSamplerShaderRegister(uint16 staticSamplerIndex) const;

		inline const void* getBlobData() const { return blobData; }
		inline uint32 getBlobSize() const { return blobSize; }

	public:
		static PipelineLayoutRef Create(const PipelineBindingDesc* bindings, uint16 bindingCount,
			const StaticSamplerDesc* staticSamplers, uint16 staticSamplerCount,
			GenericErrorMessage& errorMessage);
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

	public:
		static BlobRef Create(uint32 size);
	};

	enum class ShaderCompilationStatus : uint8
	{
		Success = 0,
		PreprocessingError,
		CompilationError,
		CompilerCallFailed,
	};

	class ShaderCompilationResult : public XLib::RefCounted
	{
	private:
		XLib::StringViewASCII preprocessorStdOut;
		XLib::StringViewASCII compilerStdOut;

		XLib::StringViewASCII d3dShaderHashStr;

		BlobRef preprocessedSourceBlob;
		BlobRef bytecodeBlob;
		BlobRef disassemblyBlob;
		BlobRef d3dPDBBlob;

		ShaderCompilationStatus status = ShaderCompilationStatus::Success;

	private:
		ShaderCompilationResult() = default;
		virtual ~ShaderCompilationResult() override = default;

	public:
		inline ShaderCompilationStatus getStatus() const { return status; }

		inline XLib::StringViewASCII getPreprocessorStdOut() const { return preprocessorStdOut; }
		inline XLib::StringViewASCII getCompilerStdOut() const { return compilerStdOut; }

		inline XLib::StringViewASCII getD3DShaderHash() const { return d3dShaderHashStr; }

		inline const Blob* getPreprocessedSourceBlob() const { return preprocessedSourceBlob.get(); }
		inline const Blob* getBytecodeBlob() const { return bytecodeBlob.get(); }
		inline const Blob* getDisassemblyBlob() const { return disassemblyBlob.get(); }
		inline const Blob* getD3DPDBBlob() { return d3dPDBBlob.get(); }

		// ExportedCBLayoutsRef getExportedCBLayouts() const;

	public:
		struct ComposerSource
		{
			ShaderCompilationStatus status;

			XLib::StringViewASCII preprocessorStdOut;
			XLib::StringViewASCII compilerStdOut;

			const Blob* preprocessedSourceBlob;
			const Blob* bytecodeBlob;
			const Blob* disassemblyBlob;
			const Blob* d3dPDBBlob;
		};

		static ShaderCompilationResultRef Compose(const ComposerSource& source);
	};

	struct IncludeResolutionResult
	{
		XLib::StringViewASCII text;
		bool resolved;
	};

	using IncludeResolverFunc = IncludeResolutionResult(*)(void* context, XLib::StringViewASCII includeFilePath);

	bool ValidateDescriptorSetBindingName(XLib::StringViewASCII name);
	bool ValidatePipelineBindingName(XLib::StringViewASCII name);
	bool ValidateShaderEntryPointName(XLib::StringViewASCII name);

	ShaderCompilationResultRef CompileShader(
		XLib::StringViewASCII mainSourceFilePath, XLib::StringViewASCII mainSourceFileText,
		const ShaderCompilationArgs& args, const PipelineLayout& pipelineLayout,
		IncludeResolverFunc includeResolverFunc, void* includeResolverContext = nullptr);
}
