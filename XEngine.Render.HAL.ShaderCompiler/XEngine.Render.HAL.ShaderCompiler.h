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

	class CompiledDescriptorBundleLayout : public XLib::NonCopyable
	{
	private:

	public:
		const void* getBinaryBlobData() const;
		uint32 getBinaryBlobSize() const;
	};

	class CompiledPipelineLayout : public XLib::NonCopyable
	{
	private:

	public:
		const void* getBinaryBlobData() const;
		uint32 getBinaryBlobSize() const;
	};

	class CompiledShader : public XLib::NonCopyable
	{
	private:

	public:
		const void* getBinaryBlobData() const;
		uint32 getBinaryBlobSize() const;
	};

	class CompiledPipeline : public XLib::NonCopyable
	{
	private:

	public:

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
