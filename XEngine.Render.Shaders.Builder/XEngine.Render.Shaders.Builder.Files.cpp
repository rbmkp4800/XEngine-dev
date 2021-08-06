#include <XLib.h>
#include <XLib.TextStream.h>

#include "XEngine.Render.Shaders.Builder.Files.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder;

bool IsWhitespace(char c)
{
	return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

void SkipToNextLine(TextFileStreamReader& reader)
{
	for (;;)
	{
		if (reader.endOfStream())
			return;

		char c = reader.get();
		if (c == '\n')
			return;
	}
}

void SkipWhitespacesAndComments(TextFileStreamReader& reader)
{
	for (;;)
	{
		if (reader.endOfStream())
			return;

		char c = char(reader.peek());
		if (IsWhitespace(c))
			reader.get();
		else if (c == '#')
			SkipToNextLine(reader);
		else
			return;

		reader.get();
	}
}

template <typename StringType>
bool ReadStringInDoubleQuotes(TextFileStreamReader& reader, StringType& result)
{
	SkipWhitespacesAndComments(reader);

	if (reader.eof())
		return false;

	char c = char(reader.get());
	if (c != '\"')
		return false;

	for (;;)
	{
		if (reader.endOfStream())
			return false;

		c = char(reader.get());
		if (c == '\"')
			return true;
		if (c == '\n')
			return false;

		result.append(c);
	}
}

ShaderType ShaderTypeFromFirstLetter(char c)
{
	switch (c)
	{
		case 'C':	return ShaderType::CS;
		case 'V':	return ShaderType::VS;
		case 'M':	return ShaderType::MS;
		case 'A':	return ShaderType::AS;
		case 'P':	return ShaderType::PS;
		default:	return ShaderType::None;
	}
}

bool XEngine::Render::Shaders::Builder::LoadShadersListFile(const char* shadersListFilePath,
	ShadersList& shadersList, SourcesCache& sourcesCache)
{
	TextFileStreamReader reader;
	if (!reader.open(shadersListFilePath))
	{
		// Can't open file
		return false;
	}

	for (;;)
	{
		SkipWhitespacesAndComments(reader);

		if (reader.endOfStream())
			return true;

		char c = reader.get();

		if (c == ';')
			continue;

		if (c != '$')
		{
			// Expected shader decl
			return false;
		}

		ShaderType shaderType = ShaderTypeFromFirstLetter(reader.get());

		char c1 = reader.get();
		char c2 = reader.get();
		if (shaderType == ShaderType::None || c1 != 'S' || !IsWhitespace(c2))
		{
			// Invalid shader decl
			return false;
		}

		ShaderNameString shaderName;
		if (!ReadStringInDoubleQuotes(reader, shaderName))
		{
			// Expected shader name
			return false;
		}

		SourcePathString mainSourceFilePath;
		if (!ReadStringInDoubleQuotes(reader, mainSourceFilePath))
		{
			// Expected shader source path
			return false;
		}

		SkipWhitespacesAndComments(reader);
		c = reader.get();
		if (c != ';')
		{
			// Expected `;`
			return false;
		}

		const SourcesCacheEntryId mainSourceId = sourcesCache.findOrCreateEntry(mainSourceFilePath.cstr());

		ShadersListEntry* shaderEntry = shadersList.createEntry(shaderName, shaderType, mainSourceId);
		if (!shaderEntry)
		{
			// Duplicate shader
			return false;
		}
	}
}

bool XEngine::Render::Shaders::Builder::LoadPrevBuildMetadataFile(const char* metadataFilePath,
	ShadersList& shadersList, SourcesCache& sourcesCache, bool& needToRelinkPack)
{
	TextFileStreamReader reader;
	if (!reader.open(metadataFilePath))
		return false;


}