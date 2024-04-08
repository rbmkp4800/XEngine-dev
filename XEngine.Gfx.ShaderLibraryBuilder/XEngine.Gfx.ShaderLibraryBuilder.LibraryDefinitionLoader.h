#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.JSON.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Gfx.HAL.ShaderCompiler.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.LibraryDefinition.h"

namespace XEngine::Gfx::ShaderLibraryBuilder
{
	class LibraryDefinitionLoader : public XLib::NonCopyable
	{
	private:
		struct Cursor
		{
			uint32 lineNumber;
			uint32 columnNumber;
		};

		struct StaticSamplerDesc
		{
			XLib::StringViewASCII name;
			HAL::SamplerDesc desc;
		};

	private:
		LibraryDefinition& libraryDefinition;

		XLib::JSONReader jsonReader;
		const char* jsonPath = nullptr;

		XLib::ArrayList<StaticSamplerDesc> staticSamplers;	// TODO: Replace with FlatHashMap.

	private:
		void reportError(const char* message, Cursor jsonCursor);
		void reportJSONError();

		bool consumeKeyWithObjectValue(XLib::StringViewASCII& resultKey);
		bool consumeSpecificKeyWithStringValue(const char* expectedKey, XLib::StringViewASCII& resultStringValue);
		bool consumeSpecificKeyWithObjectValue(const char* expectedKey);
		bool consumeSpecificKeyWithArrayValue(const char* expectedKey);

		bool readStaticSampler(XLib::StringViewASCII staticSamplerName, Cursor jsonStaticSamplerNameCursor);
		bool readDescriptorSetLayout(XLib::StringViewASCII descriptorSetLayoutName, Cursor jsonDescriptorSetLayoutNameCursor);
		bool readPipelineLayout(XLib::StringViewASCII pipelineLayoutName, Cursor jsonPipelineLayoutNameCursor);
		bool readShader(XLib::StringViewASCII shaderName, Cursor jsonShaderNameCursor, HAL::ShaderType shaderType);

		bool readPipelineLayoutSetupProperty(HAL::ShaderCompiler::PipelineLayoutRef& resultPipelineLayout, uint64& resultPipelineLayoutNameXSH);

		inline Cursor getJSONCursor() const { return Cursor { jsonReader.getLineNumer(), jsonReader.getColumnNumer() }; };

	private:
		inline LibraryDefinitionLoader(LibraryDefinition& libraryDefinition) : libraryDefinition(libraryDefinition) {}
		~LibraryDefinitionLoader() = default;

		bool load(const char* jsonPath);

	public:
		static bool Load(LibraryDefinition& libraryDefinition, const char* jsonPath);
	};
}
