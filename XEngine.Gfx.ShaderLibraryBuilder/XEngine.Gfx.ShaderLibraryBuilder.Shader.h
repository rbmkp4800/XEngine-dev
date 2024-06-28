#include <XLib.h>
#include <XLib.String.h>
#include <XLib.RefCounted.h>

#include <XEngine.Gfx.HAL.ShaderCompiler.h>

namespace XEngine::Gfx::ShaderLibraryBuilder
{
	class Shader;
	using ShaderRef = XLib::RefCountedPtr<Shader>;

	class Shader : public XLib::RefCounted
	{
	private:
		XLib::StringViewASCII name;
		uint64 nameXSH = 0;

		HAL::ShaderCompiler::PipelineBindingLayoutRef pipelineBindingLayout = nullptr;
		uint64 pipelineBindingLayoutNameXSH = 0;

		XLib::StringViewASCII mainSourceFilename;
		HAL::ShaderCompiler::ShaderCompilationArgs compilationArgs = {};
		HAL::ShaderCompiler::BlobRef compiledBlob = nullptr;

	private:
		Shader() = default;
		virtual ~Shader() override = default;

	public:
		inline XLib::StringViewASCII getName() const { return name; }
		inline uint64 getNameXSH() const { return nameXSH; }

		inline const HAL::ShaderCompiler::PipelineBindingLayout& getPipelineBindingLayout() const { return *pipelineBindingLayout.get(); }
		inline uint64 getPipelineBindingLayoutNameXSH() const { return pipelineBindingLayoutNameXSH; }

		inline XLib::StringViewASCII getMainSourceFilename() const { return mainSourceFilename; }
		inline const HAL::ShaderCompiler::ShaderCompilationArgs& getCompilationArgs() const { return compilationArgs; }

		inline void setCompiledBlob(const HAL::ShaderCompiler::Blob* blob) { compiledBlob = (HAL::ShaderCompiler::Blob*)blob; }
		inline const HAL::ShaderCompiler::Blob& getCompiledBlob() const { return *compiledBlob.get(); }

	public:
		static ShaderRef Create(XLib::StringViewASCII name, uint64 nameXSH,
			HAL::ShaderCompiler::PipelineBindingLayout* pipelineBindingLayout, uint64 pipelineBindingLayoutNameXSH,
			XLib::StringViewASCII mainSourceFilename, const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs);
	};
}
