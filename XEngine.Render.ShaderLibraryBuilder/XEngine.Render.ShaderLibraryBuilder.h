#pragma once

#include <XLib.h>
#include <XLib.Containers.BinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>
#include <XLib.RefCounted.h>

#include <XEngine.Render.HAL.Common.h>
#include <XEngine.Render.HAL.ShaderCompiler.h>

namespace XEngine::Render::ShaderLibraryBuilder
{
	class Pipeline;
	using PipelineRef = XLib::RefCountedPtr<Pipeline>;

	class Pipeline : public XLib::RefCounted
	{
	private:
		XLib::StringViewASCII name;

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
		XLib::FlatBinaryTreeMap<uint64, HAL::ShaderCompiler::DescriptorSetLayoutRef> descriptorSetLayouts;
		XLib::FlatBinaryTreeMap<uint64, HAL::ShaderCompiler::PipelineLayoutRef> pipelineLayouts;
		XLib::FlatBinaryTreeMap<uint64, PipelineRef> pipelines;
	};
}
