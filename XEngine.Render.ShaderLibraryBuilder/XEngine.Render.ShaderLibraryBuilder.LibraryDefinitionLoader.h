#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include "XJSON.h"
#include "XEngine.Render.ShaderLibraryBuilder.h"

namespace XEngine::Render::ShaderLibraryBuilder
{
	class LibraryDefinitionLoader : public XLib::NonCopyable
	{
	private:
		LibraryDefinition& libraryDefinition;

		XJSON::MemoryStreamParser xjsonParser;
		const char* xjsonPath = nullptr;

	private:
		void reportError(const char* message, const XJSON::Location& location);
		void reportXJSONError(const XJSON::ParserError& xjsonError);

		bool readDescriptorSetLayoutDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty);
		bool readPipelineLayoutDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty);
		bool readGraphicsPipelineDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty);
		bool readComputePipelineDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty);

		bool readPipelineLayoutSetupProperty(const XJSON::KeyValue& xjsonOuterProperty,
			HAL::ShaderCompiler::PipelineLayoutRef& resultPipelineLayout, uint64& resultPipelineLayoutNameXSH);
		bool readShaderSetupProperty(const XJSON::KeyValue& xjsonOuterProperty, HAL::ShaderCompiler::ShaderDesc& resultShader);

	public:
		inline LibraryDefinitionLoader(LibraryDefinition& libraryDefinition) : libraryDefinition(libraryDefinition) {}
		~LibraryDefinitionLoader() = default;

		bool load(const char* xjsonPathCStr);
	};
}
