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

		HAL::ShaderCompiler::PipelineLayoutRef pipelineLayout = nullptr;
		uint64 pipelineLayoutNameXSH = 0;

		XLib::StringViewASCII mainSourceFilename;
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

		inline XLib::StringViewASCII getMainSourceFilename() const { return mainSourceFilename; }
		inline const HAL::ShaderCompiler::ShaderCompilationArgs& getCompilationArgs() const { return compilationArgs; }

		inline void setCompiledBlob(const HAL::ShaderCompiler::Blob* blob) { compiledBlob = (HAL::ShaderCompiler::Blob*)blob; }
		inline const HAL::ShaderCompiler::Blob& getCompiledBlob() const { return *compiledBlob.get(); }

	public:
		static ShaderRef Create(XLib::StringViewASCII name, uint64 nameXSH,
			HAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
			XLib::StringViewASCII mainSourceFilename, const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs);
	};
}
