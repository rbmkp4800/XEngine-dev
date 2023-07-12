#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>
#include <XLib.RefCounted.h>

#include <XEngine.GfxHAL.Common.h>
#include <XEngine.GfxHAL.ShaderCompiler.h>

// TODO: Use hash maps in LibraryDefinition. Yes, I am retarded.

namespace XEngine::Render::ShaderLibraryBuilder
{
	class Pipeline;
	using PipelineRef = XLib::RefCountedPtr<Pipeline>;

	class Pipeline : public XLib::RefCounted
	{
	private:
		XLib::StringViewASCII name;

		GfxHAL::ShaderCompiler::PipelineLayoutRef pipelineLayout;
		uint64 pipelineLayoutNameXSH = 0;

		struct
		{
			GfxHAL::ShaderCompiler::ShaderDesc shader;
			GfxHAL::ShaderCompiler::BlobRef compiledBlob;
		} compute = {};

		struct
		{
			GfxHAL::ShaderCompiler::GraphicsPipelineShaders shaders;
			GfxHAL::ShaderCompiler::GraphicsPipelineSettings settings;
			GfxHAL::ShaderCompiler::GraphicsPipelineCompiledBlobs compiledBlobs;
		} graphics = {};

		bool isGraphics = false;

	private:
		Pipeline() = default;
		virtual ~Pipeline() override = default;

	public:
		inline XLib::StringViewASCII getName() const { return name; }

		inline GfxHAL::ShaderCompiler::PipelineLayout* getPipelineLayout() const { return pipelineLayout.get(); }
		inline uint64 getPipelineLayoutNameXSH() const { return pipelineLayoutNameXSH; }

		inline GfxHAL::ShaderCompiler::ShaderDesc getComputeShader() const { return compute.shader; }
		inline GfxHAL::ShaderCompiler::BlobRef& computeShaderCompiledBlob() { return compute.compiledBlob; }
		inline const GfxHAL::ShaderCompiler::BlobRef& computeShaderCompiledBlob() const { return compute.compiledBlob; }

		inline GfxHAL::ShaderCompiler::GraphicsPipelineShaders getGraphicsShaders() const { return graphics.shaders; }
		inline GfxHAL::ShaderCompiler::GraphicsPipelineSettings getGraphicsSettings() const { return graphics.settings; }
		inline GfxHAL::ShaderCompiler::GraphicsPipelineCompiledBlobs& graphicsCompiledBlobs() { return graphics.compiledBlobs; }
		inline const GfxHAL::ShaderCompiler::GraphicsPipelineCompiledBlobs& graphicsCompiledBlobs() const { return graphics.compiledBlobs; }

		inline bool isGraphicsPipeline() const { return isGraphics; }

	public:

		// These methods pull all arrays / strings from input and store them locally.
		// Shader text is not stored.

		static PipelineRef CreateGraphics(XLib::StringViewASCII name,
			GfxHAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
			const GfxHAL::ShaderCompiler::GraphicsPipelineShaders& shaders,
			const GfxHAL::ShaderCompiler::GraphicsPipelineSettings& settings);

		static PipelineRef CreateCompute(XLib::StringViewASCII name,
			GfxHAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
			const GfxHAL::ShaderCompiler::ShaderDesc& shader);
	};

	struct LibraryDefinition
	{
		struct DescriptorSetLayout
		{
			uint64 nameXSH;
			GfxHAL::ShaderCompiler::DescriptorSetLayoutRef ref;
		};

		struct PipelineLayout
		{
			uint64 nameXSH;
			GfxHAL::ShaderCompiler::PipelineLayoutRef ref;
		};

		struct Pipeline
		{
			uint64 nameXSH;
			PipelineRef ref;
		};

		XLib::ArrayList<DescriptorSetLayout> descriptorSetLayouts;
		XLib::ArrayList<PipelineLayout> pipelineLayouts;
		XLib::ArrayList<Pipeline> pipelines;
	};
}
