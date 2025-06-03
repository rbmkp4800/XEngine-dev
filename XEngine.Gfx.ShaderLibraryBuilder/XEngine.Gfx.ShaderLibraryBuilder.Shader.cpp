#include "XEngine.Gfx.ShaderLibraryBuilder.Shader.h"

using namespace XLib;
using namespace XEngine::Gfx::ShaderLibraryBuilder;

ShaderRef Shader::Create(StringViewASCII name, uint64 nameXSH,
	HAL::ShaderCompiler::PipelineLayout* pipelineLayout, StringViewASCII pipelineLayoutName, uint64 pipelineLayoutNameXSH,
	XLib::StringViewASCII mainSourceFilePath, const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs)
{
	// I should not be allowed to code. Sorry :(

	uintptr memoryBlockSizeAccum = sizeof(Shader);

	const uintptr nameStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += name.getLength() + 1;

	const uintptr pipelineLayoutNameStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += pipelineLayoutName.getLength() + 1;

	const uintptr mainSourceFilePathStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += mainSourceFilePath.getLength() + 1;

	const uintptr entryPointNameStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += compilationArgs.entryPointName.getLength() + 1;

	const uintptr memoryBlockSize = memoryBlockSizeAccum;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize);

	memoryCopy((char*)memoryBlock + nameStrOffset, name.getData(), name.getLength());
	memoryCopy((char*)memoryBlock + pipelineLayoutNameStrOffset, pipelineLayoutName.getData(), pipelineLayoutName.getLength());
	memoryCopy((char*)memoryBlock + mainSourceFilePathStrOffset, mainSourceFilePath.getData(), mainSourceFilePath.getLength());
	memoryCopy((char*)memoryBlock + entryPointNameStrOffset, compilationArgs.entryPointName.getData(), compilationArgs.entryPointName.getLength());

	Shader& resultObject = *(Shader*)memoryBlock;
	construct(resultObject);
	resultObject.name = StringViewASCII((char*)memoryBlock + nameStrOffset, name.getLength());
	resultObject.nameXSH = nameXSH;
	resultObject.pipelineLayout = pipelineLayout;
	resultObject.pipelineLayoutName = StringViewASCII((char*)memoryBlock + pipelineLayoutNameStrOffset, pipelineLayoutName.getLength());
	resultObject.pipelineLayoutNameXSH = pipelineLayoutNameXSH;
	resultObject.mainSourceFilePath = StringViewASCII((char*)memoryBlock + mainSourceFilePathStrOffset, mainSourceFilePath.getLength());
	resultObject.compilationArgs.entryPointName = StringViewASCII((char*)memoryBlock + entryPointNameStrOffset, compilationArgs.entryPointName.getLength());
	resultObject.compilationArgs.shaderType = compilationArgs.shaderType;

	return ShaderRef(&resultObject);
}
