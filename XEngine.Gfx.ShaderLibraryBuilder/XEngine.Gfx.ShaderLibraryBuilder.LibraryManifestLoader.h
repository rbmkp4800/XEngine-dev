#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.JSON.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Gfx.HAL.ShaderCompiler.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.Library.h"

namespace XEngine::Gfx::ShaderLibraryBuilder
{
	class LibraryManifestLoader : public XLib::NonCopyable
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
		Library& library;

		XLib::JSONReader jsonReader;
		const char* jsonPathCStr = nullptr;

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

		inline Cursor getJSONCursor() const { return Cursor { jsonReader.getLineNumer(), jsonReader.getColumnNumer() }; };

	private:
		inline LibraryManifestLoader(Library& library) : library(library) {}
		~LibraryManifestLoader() = default;

		bool load(const char* jsonPathCStr);

	public:
		static bool Load(Library& library, const char* jsonPathCStr);
	};
}
