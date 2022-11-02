#pragma once

#include <XLib.h>
#include <XLib.Containers.IntrusiveBinaryTree.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

#include <XEngine.Render.HAL.ShaderCompiler.h>

#include "XEngine.Render.Shaders.Builder.PipelineLayoutList.h"
#include "XEngine.Render.Shaders.Builder.SourceCache.h"

namespace XEngine::Render::Shaders::Builder_
{
	class ShaderList;

	class Shader : public XLib::NonCopyable
	{
		friend ShaderList;

	private:
		XLib::IntrusiveBinaryTreeNodeHook searchTreeHook;

		Source* source = nullptr;
		const char* entryPointName = nullptr;
		const PipelineLayout* pipelineLayout = nullptr;
		HAL::ShaderCompiler::ShaderType type = HAL::ShaderCompiler::ShaderType::Undefined;

		HAL::ShaderCompiler::Blob compiledShaderBlob;

	private:
		Shader() = default;
		~Shader() = default;

	public:
		bool compile();

		inline const Source& getSource() const { return *source; }
		inline const char* getEntryPointName() const { return entryPointName; }
		inline const PipelineLayout& getPipelineLayout() const { return *pipelineLayout; }
		inline HAL::ShaderCompiler::ShaderType getType() const { return type; }
		inline const HAL::ShaderCompiler::Blob& getCompiledBlob() const { return compiledShaderBlob; }
	};

	enum class ShaderCreationStatus
	{
		Success = 0,
		Failure_ShaderTypeMismatch,
	};

	struct ShaderCreationResult
	{
		ShaderCreationStatus status;
		Shader* shader;
	};

	class ShaderList : public XLib::NonCopyable
	{
	private:
		struct EntrySearchKey;
		struct EntrySearchTreeComparator abstract final
		{
			static ordering Compare(const Shader& left, const Shader& right);
			static ordering Compare(const Shader& left, const EntrySearchKey& right);
		};

		using EntrySearchTree = XLib::IntrusiveBinaryTree<Shader, &Shader::searchTreeHook, EntrySearchTreeComparator>;

	private:
		EntrySearchTree entrySearchTree;

	public:
		using Iterator = EntrySearchTree::Iterator;

	public:
		ShaderList() = default;
		~ShaderList() = default;

		ShaderCreationResult findOrCreate(HAL::ShaderCompiler::ShaderType type,
			Source& source, XLib::StringViewASCII entryPointName, const PipelineLayout& pipelineLayout);

		inline Iterator begin() { return entrySearchTree.begin(); }
		inline Iterator end() { return entrySearchTree.end(); }
	};
}
