#include <XLib.String.h>
#include <XLib.Text.h>

#include "XEngine.Render.Shaders.Builder.ShaderList.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder_;

bool Shader::compile()
{
	StringViewASCII sourceText = {};
	if (!source->retrieveText(sourceText))
	{
		TextWriteFmtStdOut("Failed to read file \"", source->getLocalPath(), "\"\n");
		return false;
	}

	return HAL::ShaderCompiler::Host::CompileShader(HAL::ShaderCompiler::Platform::D3D12,
		pipelineLayout->getCompiledBlob(), pipelineLayout->getMetadataBlob(),
		sourceText.getData(), uint32(sourceText.getLength()),
		type, source->getLocalPathCStr(), entryPointName, compiledShaderBlob);
}

struct ShaderList::EntrySearchKey
{
	const Source& source;
	StringViewASCII entryPointName;
	const PipelineLayout& pipelineLayout;

	static inline EntrySearchKey Construct(const Shader& shader)
	{
		return EntrySearchKey { *shader.source, shader.entryPointName, *shader.pipelineLayout };
	}

	// Comparison order: source path -> entry point name -> pipeline layout
	static inline ordering Compare(const EntrySearchKey& left, const EntrySearchKey& right)
	{
		const ordering sourceOrdering = Source::CompareOrdered(left.source, right.source);
		if (sourceOrdering != ordering::equivalent)
			return sourceOrdering;
		const ordering entryPointOrdering = String::CompareOrdered(left.entryPointName, right.entryPointName);
		if (entryPointOrdering != ordering::equivalent)
			return entryPointOrdering;
		return PipelineLayout::CompareOrdered(left.pipelineLayout, right.pipelineLayout);
	}
};

ordering ShaderList::EntrySearchTreeComparator::Compare(const Shader& left, const Shader& right)
{
	return EntrySearchKey::Compare(EntrySearchKey::Construct(left), EntrySearchKey::Construct(right));
}

ordering ShaderList::EntrySearchTreeComparator::Compare(const Shader& left, const EntrySearchKey& right)
{
	return EntrySearchKey::Compare(EntrySearchKey::Construct(left), right);
}

ShaderCreationResult ShaderList::findOrCreate(HAL::ShaderCompiler::ShaderType type,
	Source& source, XLib::StringViewASCII entryPointName, const PipelineLayout& pipelineLayout)
{
	// TODO: Validate shader type value.

	const EntrySearchKey existingShaderSearchKey { source, entryPointName, pipelineLayout };
	if (Shader* existingShader = entrySearchTree.find(existingShaderSearchKey))
	{
		if (existingShader->getType() != type)
			return ShaderCreationResult { ShaderCreationStatus::Failure_ShaderTypeMismatch };
		return ShaderCreationResult { ShaderCreationStatus::Success, existingShader };
	}

	const uint32 memoryBlockSize = sizeof(Shader) + uint32(entryPointName.getLength()) + 1;
	byte* shaderMemory = (byte*)SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(shaderMemory, 0, memoryBlockSize);

	char* entryPointNameMemory = (char*)(shaderMemory + sizeof(Shader));
	memoryCopy(entryPointNameMemory, entryPointName.getData(), entryPointName.getLength());
	entryPointNameMemory[entryPointName.getLength()] = '\0';

	Shader& shader = *(Shader*)shaderMemory;
	construct(shader);
	shader.source = &source;
	shader.entryPointName = entryPointNameMemory;
	shader.pipelineLayout = &pipelineLayout;
	shader.type = type;

	entrySearchTree.insert(shader);
	entryCount++;

	return ShaderCreationResult { ShaderCreationStatus::Success, &shader };
}
