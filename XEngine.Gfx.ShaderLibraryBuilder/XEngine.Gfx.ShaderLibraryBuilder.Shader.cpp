#include "XEngine.Gfx.ShaderLibraryBuilder.Shader.h"

using namespace XLib;
using namespace XEngine::Gfx::ShaderLibraryBuilder;

ShaderRef Shader::Create(StringViewASCII name, uint64 nameXSH,
	HAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
	XLib::StringViewASCII mainSourceFilename, const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs)
{
	// I should not be allowed to code. Sorry :(

	uintptr memoryBlockSizeAccum = sizeof(Shader);

	const uintptr nameStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += name.getLength() + 1;

	const uintptr mainSourceFilenameStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += mainSourceFilename.getLength() + 1;

	const uintptr entryPointNameStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += compilationArgs.entryPointName.getLength() + 1;

	const uintptr memoryBlockSize = memoryBlockSizeAccum;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize);

	memoryCopy((char*)memoryBlock + nameStrOffset, name.getData(), name.getLength());
	memoryCopy((char*)memoryBlock + mainSourceFilenameStrOffset, mainSourceFilename.getData(), mainSourceFilename.getLength());
	memoryCopy((char*)memoryBlock + entryPointNameStrOffset, compilationArgs.entryPointName.getData(), compilationArgs.entryPointName.getLength());

	Shader& resultObject = *(Shader*)memoryBlock;
	construct(resultObject);
	resultObject.name = StringViewASCII((char*)memoryBlock + nameStrOffset, name.getLength());
	resultObject.nameXSH = nameXSH;
	resultObject.pipelineLayout = pipelineLayout;
	resultObject.pipelineLayoutNameXSH = pipelineLayoutNameXSH;
	resultObject.mainSourceFilename = StringViewASCII((char*)memoryBlock + mainSourceFilenameStrOffset, mainSourceFilename.getLength());
	resultObject.compilationArgs.entryPointName = StringViewASCII((char*)memoryBlock + entryPointNameStrOffset, compilationArgs.entryPointName.getLength());
	resultObject.compilationArgs.shaderType = compilationArgs.shaderType;

	return ShaderRef(&resultObject);
}
