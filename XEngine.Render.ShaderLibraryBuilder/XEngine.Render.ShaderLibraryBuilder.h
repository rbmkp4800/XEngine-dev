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

	private:
		Pipeline() = default;
		virtual ~Pipeline() override = default;

	public:
		inline XLib::StringViewASCII getName() const { return name; }

	public:
		HAL::ShaderCompiler::PipelineLayoutRef pipelineLayout;
		uint64 pipelineLayoutNameXSH = 0;

		// NOTE: For all 'ShaderDesc's source text is zero. Only source path is valid.

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

	public:
		static PipelineRef Create(XLib::StringViewASCII name);
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
