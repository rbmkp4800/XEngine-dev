#include <XLib.h>
#include <XLib.String.h>
#include <XLib.Text.h>

#include "XEngine.Render.Shaders.Builder.h"

using namespace XLib;
using namespace XEngine::Render::Shaders;

int main()
{
	// -slist="/XEngine.Render.Shaders/_ShadersList.txt" -intrmdir="/Build.Intermediate/XEngine.Render.Shaders/" -out="/Build/XEngine.Render.Shaders.xspk" -inc

#if 0
	const char* args = ...;

	TextStreamReader argsReader(args);

	bool useIncrementalBuild = false;
	String shadersListFilePath;
	String intermediateDirPath;
	String outputFilePath;

	for (;;)
	{
		InplaceString32 argument;

		FormatRead(argsReader, Fmt::RdWS(), '-', Fmt::RdStr(argument));

		if (CompareStrings(argument, "slist") == 0)
		{
			FormatRead(argsReader, '=', Fmt::RdStr(shadersListFilePath));
		}
		else if (CompareStrings(argument, "intrmdir") == 0)
		{
			FormatRead(argsReader, '=', Fmt::RdStr(intermediateDirPath));
		}
		else if (CompareStrings(argument, "out") == 0)
		{
			FormatRead(argsReader, '=', Fmt::RdStr(outputFilePath));
		}
		else if (CompareStrings(argument, "inc") == 0)
		{
			useIncrementalBuild = true;
		}
	}
#endif

	Builder builder;
	builder.run("..\\Build\\shaderpack");

#if 0
	if (!LoadShadersListFile(shadersListFilePath.cstr(), shadersList, sourcesCache))
		return -1;

	if (shadersList.isEmpty())
	{
		// No shaders declared
		return 0;
	}

	bool relinkPack = false;
	const bool metadataLoadResult = LoadPrevBuildMetadataFile(, shadersList, sourcesCache, relinkPack);
#endif

	

	return 0;
}
