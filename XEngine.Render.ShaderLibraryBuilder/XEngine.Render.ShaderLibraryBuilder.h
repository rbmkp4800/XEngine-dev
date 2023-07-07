#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>
#include <XLib.RefCounted.h>

#include <XEngine.Render.HAL.Common.h>
#include <XEngine.Render.HAL.ShaderCompiler.h>

// TODO: Use hash maps in LibraryDefinition. Yes, I am retarded.

namespace XEngine::Render::ShaderLibraryBuilder
{
	class Pipeline;
	using PipelineRef = XLib::RefCountedPtr<Pipeline>;

	class Pipeline : public XLib::RefCounted
	{
	private:
		XLib::StringViewASCII name;

		HAL::ShaderCompiler::PipelineLayoutRef pipelineLayout;
		uint64 pipelineLayoutNameXSH = 0;

		struct
		{
			HAL::ShaderCompiler::ShaderDesc shader;
			HAL::ShaderCompiler::BlobRef compiledBlob;
		} compute = {};

		struct
		{
			HAL::ShaderCompiler::GraphicsPipelineShaders shaders;
			HAL::ShaderCompiler::GraphicsPipelineSettings settings;
			HAL::ShaderCompiler::GraphicsPipelineCompiledBlobs compiledBlobs;
		} graphics = {};

		bool isGraphics = false;

	private:
		Pipeline() = default;
		virtual ~Pipeline() override = default;

	public:
		inline XLib::StringViewASCII getName() const { return name; }

		inline HAL::ShaderCompiler::PipelineLayout* getPipelineLayout() const { return pipelineLayout.get(); }
		inline uint64 getPipelineLayoutNameXSH() const { return pipelineLayoutNameXSH; }

		inline HAL::ShaderCompiler::ShaderDesc getComputeShader() const { return compute.shader; }
		inline HAL::ShaderCompiler::BlobRef& computeShaderCompiledBlob() { return compute.compiledBlob; }
		inline const HAL::ShaderCompiler::BlobRef& computeShaderCompiledBlob() const { return compute.compiledBlob; }

		inline HAL::ShaderCompiler::GraphicsPipelineShaders getGraphicsShaders() const { return graphics.shaders; }
		inline HAL::ShaderCompiler::GraphicsPipelineSettings getGraphicsSettings() const { return graphics.settings; }
		inline HAL::ShaderCompiler::GraphicsPipelineCompiledBlobs& graphicsCompiledBlobs() { return graphics.compiledBlobs; }
		inline const HAL::ShaderCompiler::GraphicsPipelineCompiledBlobs& graphicsCompiledBlobs() const { return graphics.compiledBlobs; }

		inline bool isGraphicsPipeline() const { return this->isGraphics; }

	public:

		// These methods pull all arrays / strings from input and store them locally.
		// Shader text is not stored.

		static PipelineRef CreateGraphics(XLib::StringViewASCII name,
			HAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
			const HAL::ShaderCompiler::GraphicsPipelineShaders& shaders,
			const HAL::ShaderCompiler::GraphicsPipelineSettings& settings);

		static PipelineRef CreateCompute(XLib::StringViewASCII name,
			HAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
			const HAL::ShaderCompiler::ShaderDesc& shader);
	};

	struct LibraryDefinition
	{
		struct DescriptorSetLayout
		{
			uint64 nameXSH;
			HAL::ShaderCompiler::DescriptorSetLayoutRef ref;
		};

		struct PipelineLayout
		{
			uint64 nameXSH;
			HAL::ShaderCompiler::PipelineLayoutRef ref;
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
