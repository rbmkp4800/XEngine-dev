#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>
#include <XLib.RefCounted.h>

#include <XEngine.Gfx.HAL.Shared.h>
#include <XEngine.Gfx.HAL.ShaderCompiler.h>

// TODO: Use hash maps in LibraryDefinition. Yes, I am retarded.

namespace XEngine::Gfx::ShaderLibraryBuilder
{
	class Shader;
	using ShaderRef = XLib::RefCountedPtr<Shader>;

	class Shader : public XLib::RefCounted
	{
	private:
		XLib::StringViewASCII name;
		uint64 nameXSH = 0;

		HAL::ShaderCompiler::PipelineLayoutRef pipelineLayout = nullptr;
		uint64 pipelineLayoutNameXSH = 0;

		HAL::ShaderCompiler::ShaderCompilationArgs compilationArgs = {};
		HAL::ShaderCompiler::BlobRef compiledBlob = nullptr;

	private:
		Shader() = default;
		virtual ~Shader() override = default;

	public:
		inline XLib::StringViewASCII getName() const { return name; }
		inline uint64 getNameXSH() const { return nameXSH; }

		inline const HAL::ShaderCompiler::PipelineLayout& getPipelineLayout() const { return *pipelineLayout.get(); }
		inline uint64 getPipelineLayoutNameXSH() const { return pipelineLayoutNameXSH; }

		inline const HAL::ShaderCompiler::ShaderCompilationArgs& getCompilationArgs() const { return compilationArgs; }

		inline void setCompiledBlob(const HAL::ShaderCompiler::Blob* blob) { compiledBlob = (HAL::ShaderCompiler::Blob*)blob; }
		inline const HAL::ShaderCompiler::Blob& getCompiledBlob() const { return *compiledBlob.get(); }

	public:
		static ShaderRef Create(XLib::StringViewASCII name, uint64 nameXSH,
			HAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
			const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs);
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

		XLib::ArrayList<DescriptorSetLayout> descriptorSetLayouts;
		XLib::ArrayList<PipelineLayout> pipelineLayouts;
		XLib::ArrayList<ShaderRef> shaders;
	};
}
