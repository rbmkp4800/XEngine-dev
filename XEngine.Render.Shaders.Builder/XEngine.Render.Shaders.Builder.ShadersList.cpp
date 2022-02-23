#include <XLib.CRC.h>
#include <XLib.String.h>

#include "XEngine.Render.Shaders.Builder.ShadersList.h"

using namespace XLib;
using namespace XEngine::Render::HAL;
using namespace XEngine::Render::Shaders::Builder_;

// Shader //////////////////////////////////////////////////////////////////////////////////////////

bool Shader::compile()
{
	StringView sourceText = {};
	if (!source.retrieveText(sourceText))
		return false;

	return ShaderCompiler::Host::CompileShader(ShaderCompiler::Platform::D3D12, pipelineLayout.getCompiled(),
		type, source.getLocalPathCStr(), sourceText.getData(), uint32(sourceText.getLength()), compiledShader);
}

// ShadersList /////////////////////////////////////////////////////////////////////////////////////

struct ShadersList::EntrySearchKey
{
	const SourceFile& source;
	StringView entryPointName;
	const PipelineLayout& pipelineLayout;

	static inline EntrySearchKey Construct(const Shader& shader)
	{
		return EntrySearchKey { shader.source, shader.entryPointName, shader.pipelineLayout };
	}

	// Comparison order: source path -> entry point name -> pipeline layout
	static inline ordering Compare(const EntrySearchKey& left, const EntrySearchKey& right)
	{
		const ordering sourceOrdering = SourceFile::CompareOrdered(left.source, right.source);
		if (sourceOrdering != ordering::equivalent)
			return sourceOrdering;
		const ordering entryPointOrdering = CompareStringsOrdered(left.entryPointName, right.entryPointName);
		if (entryPointOrdering != ordering::equivalent)
			return entryPointOrdering;
		return PipelineLayout::CompareOrdered(left.pipelineLayout, right.pipelineLayout);
	}
};

ordering ShadersList::EntriesSearchTreeComparator::Compare(const Shader& left, const Shader& right)
{
	return EntrySearchKey::Compare(EntrySearchKey::Construct(left), EntrySearchKey::Construct(right));
}

ordering ShadersList::EntriesSearchTreeComparator::Compare(const Shader& left, const EntrySearchKey& right)
{
	return EntrySearchKey::Compare(EntrySearchKey::Construct(left), right);
}

ShadersList::EntryCreationResult ShadersList::createEntryIfAbsent(ShaderCompiler::ShaderType type,
	SourceFile& source, XLib::StringView entryPointName, const PipelineLayout& pipelineLayout)
{
	// TODO: Validate shader type value.

	EntrySearchKey existingEntrySearchKey { source, entryPointName, pipelineLayout };
	const EntriesSearchTree::Iterator existingItemIt = entriesSearchTree.find(existingEntrySearchKey);
	if (existingItemIt)
	{
		if (existingItemIt->getType() != type)
			EntryCreationResult { nullptr, EntryCreationStatus::Failure_ShaderAlreadyCreatedWithOtherType };
		return EntryCreationResult { existingItemIt, EntryCreationStatus::Success };
	}

	Shader& shader = entriesStorageList.emplaceBack(source, type, entryPointName, pipelineLayout);
	entriesSearchTree.insert(shader);

	return EntryCreationResult { &shader, EntryCreationStatus::Success };
}
