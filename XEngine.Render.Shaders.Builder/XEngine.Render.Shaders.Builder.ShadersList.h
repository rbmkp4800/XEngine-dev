#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

#include "XEngine.Render.Shaders.Builder.PipelineLayoutsList.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

namespace XEngine::Render::Shaders::Builder_
{
	class ShadersList;

	class Shader : public XLib::NonCopyable
	{
		friend ShadersList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		SourceFile& source;
		XLib::InplaceString64 entryPointName;
		const PipelineLayout& pipelineLayout;
		HAL::ShaderCompiler::ShaderType type = HAL::ShaderCompiler::ShaderType::Undefined;

		HAL::ShaderCompiler::CompiledShader compiledShader;

	private:
		~Shader() = default;

	public:
		inline Shader(SourceFile& source, HAL::ShaderCompiler::ShaderType type,
			XLib::StringView entryPointName, const PipelineLayout& pipelineLayout);

		bool compile();

		inline HAL::ShaderCompiler::ShaderType getType() const { return type; }
		inline const HAL::ShaderCompiler::CompiledShader& getCompiled() const { return compiledShader; }
	};

	class ShadersList : public XLib::NonCopyable
	{
	private:
		struct EntriesSearchTreeComparator;
		struct EntrySearchKey;

		using EntriesSearchTree = XLib::IntrusiveBinaryTree<Shader, &Shader::searchTreeHook, EntriesSearchTreeComparator>;
		using EntriesStorageList = XLib::FixedLogSegmentedArrayList<Shader, 5, 16>;

	private:
		EntriesSearchTree entriesSearchTree;
		EntriesStorageList entriesStorageList;

	public:
		using Iterator = EntriesSearchTree::Iterator;

		enum class EntryCreationStatus
		{
			Success = 0,
			Failure_ShaderExistsButHasOtherType,
		};

		struct EntryCreationResult
		{
			Shader* entry;
			EntryCreationStatus status;
		};

	public:
		ShadersList() = default;
		~ShadersList() = default;

		EntryCreationResult createIfAbsent(HAL::ShaderCompiler::ShaderType type,
			SourceFile& source, XLib::StringView entryPointName, const PipelineLayout& pipelineLayout);

		inline bool isEmpty() const { return entriesStorageList.isEmpty(); }
		inline uint32 getSize() const { return entriesStorageList.getSize(); }

		inline Iterator begin() { return entriesSearchTree.begin(); }
		inline Iterator end() { return entriesSearchTree.end(); }
	};

	struct ShadersList::EntriesSearchTreeComparator abstract final
	{
		static ordering Compare(const Shader& left, const Shader& right);
		static ordering Compare(const Shader& left, const EntrySearchKey& right);
	};
}
