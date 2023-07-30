#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.JSON.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Gfx.HAL.ShaderCompiler.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.h"

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

		struct VertexInputLayoutDesc
		{
			XLib::StringViewASCII name;
			HAL::ShaderCompiler::VertexBufferDesc buffers[HAL::MaxVertexBufferCount];
			uint32 bindingsOffsetInAccumBuffer;
			uint8 bindingCount;
		};

	private:
		LibraryDefinition& libraryDefinition;

		XLib::JSONReader jsonReader;
		const char* jsonPath = nullptr;

		XLib::ArrayList<StaticSamplerDesc> staticSamplers;	// TODO: Replace with FlatHashMap.
		XLib::ArrayList<VertexInputLayoutDesc> vertexInputLayouts;	// TODO: Replace with FlatHashMap.
		XLib::ArrayList<HAL::ShaderCompiler::VertexBindingDesc> vertexBindingsAccumBuffer;

	private:
		void reportError(const char* message, Cursor jsonCursor);
		void reportJSONError();

		bool consumeKeyWithObjectValue(XLib::StringViewASCII& resultKey);
		bool consumeSpecificKeyWithStringValue(const char* expectedKey, XLib::StringViewASCII& resultStringValue);
		bool consumeSpecificKeyWithObjectValue(const char* expectedKey);
		bool consumeSpecificKeyWithArrayValue(const char* expectedKey);

		bool readStaticSampler(XLib::StringViewASCII staticSamplerName, Cursor jsonStaticSamplerNameCursor);
		bool readVertexInputLayout(XLib::StringViewASCII vertexInputLayoutName, Cursor jsonVertexInputLayoutNameCursor);
		bool readDescriptorSetLayout(XLib::StringViewASCII descriptorSetLayoutName, Cursor jsonDescriptorSetLayoutNameCursor);
		bool readPipelineLayout(XLib::StringViewASCII pipelineLayoutName, Cursor jsonPipelineLayoutNameCursor);
		bool readGraphicsPipeline(XLib::StringViewASCII graphicsPipelineName, Cursor jsonGraphicsPipelineNameCursor);
		bool readComputePipeline(XLib::StringViewASCII computePipelineName, Cursor jsonComputePipelineNameCursor);

		bool readPipelineLayoutSetupProperty(HAL::ShaderCompiler::PipelineLayoutRef& resultPipelineLayout, uint64& resultPipelineLayoutNameXSH);
		bool readShaderSetupObject(HAL::ShaderCompiler::ShaderDesc& resultShader);

		inline Cursor getJSONCursor() const { return Cursor { jsonReader.getLineNumer(), jsonReader.getColumnNumer() }; };

	private:
		inline LibraryDefinitionLoader(LibraryDefinition& libraryDefinition) : libraryDefinition(libraryDefinition) {}
		~LibraryDefinitionLoader() = default;

		bool load(const char* jsonPath);

	public:
		static bool Load(LibraryDefinition& libraryDefinition, const char* jsonPath);
	};
}
