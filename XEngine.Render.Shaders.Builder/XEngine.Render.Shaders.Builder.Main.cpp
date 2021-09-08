#include <XLib.h>
#include <XLib.FileSystem.h>
#include <XLib.String.h>
#include <XLib.Text.h>
#include <XLib.Time.h>

#include "XEngine.Render.Shaders.Builder.Common.h"
#include "XEngine.Render.Shaders.Builder.CompilerDXC.h"
#include "XEngine.Render.Shaders.Builder.Files.h"
#include "XEngine.Render.Shaders.Builder.RootSignaturesList.h"
#include "XEngine.Render.Shaders.Builder.ShadersList.h"
#include "XEngine.Render.Shaders.Builder.SourcesCache.h"

using namespace XLib;
using namespace XEngine::Render::Shaders::Builder;

#if 0

static void CheckIfShaderRequiresCompilation(ShadersListEntry& shader,
	SourcesCache& sourcesCache, const char* rootPath, TimePoint& upToDateObjectWriteTime)
{
	upToDateObjectWriteTime = InvalidTimePoint;

	// Already marked for compilation
	if (shader.isCompilationRequired())
		return;

	const TimePoint objectWriteTime = ...;
	if (objectWriteTime == InvalidTimePoint)
	{
		shader.setCompilationRequired();
		return;
	}

	const SourcesCacheEntryId sourceMainId = shader.getSourceMain();
	const TimePoint sourceMainWriteTime = sourcesCache.getEntry(sourceMainId).checkWriteTime(rootPath);
	if (sourceMainWriteTime == InvalidTimePoint)
	{
		shader.setCompilationRequired();
		return;
	}

	// Go through all dependencies checking timestamps
	TimePoint latestSourceDependencyWriteTime = 0;
	for (uint16 i = 0; i < shader.getSourceDependencyCount(); i++)
	{
		const SourcesCacheEntryId id = shader.getSourceDependency(i);
		const TimePoint writeTime = sourcesCache.getEntry(id).checkWriteTime(rootPath);

		// InvalidTimePoint will propagate here
		latestSourceDependencyWriteTime = max<TimePoint>(latestSourceDependencyWriteTime, writeTime);
	}

	const TimePoint latestSourceWriteTime = max(sourceMainWriteTime, latestSourceDependencyWriteTime);

	if (latestSourceWriteTime > objectWriteTime)
	{
		shader.setCompilationRequired();
		return;
	}

	upToDateObjectWriteTime = objectWriteTime;
}

#endif

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

	RootSignaturesList rootSignaturesList;
	ShadersList shadersList;
	SourcesCache sourcesCache;

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

	ArrayList<Shader*> shadersToCompile;

#if 0
	const bool rebuildAll = !metadataLoadResult;
	if (rebuildAll)
	{
		for (ShadersListEntry& shader : shadersList)
			shadersToCompile.pushBack(&shader);
	}
	else
	{
		const TimePoint packWriteTime = FileSystem::GetFileLastWriteTime(outputFilePath.cstr());

		// Relink pack if it does not exist
		relinkPack |= packWriteTime == InvalidTimePoint;

		TimePoint latestObjectWriteTime = 0;

		for (ShadersListEntry& shader : shadersList)
		{
			TimePoint upToDateObjectWriteTime = 0;
			CheckIfShaderRequiresCompilation(shader, sourcesCache, rootPath.cstr(), upToDateObjectWriteTime);

			if (shader.isCompilationRequired())
				shadersToCompile.pushBack(&shader);
			else
				latestObjectWriteTime = max(latestObjectWriteTime, upToDateObjectWriteTime);
		}

		// Relink pack if all shaders are up to date but pack is out of date
		if (shadersToCompile.isEmpty())
			relinkPack |= latestObjectWriteTime > packWriteTime;
	}

	relinkPack |= !shadersToCompile.isEmpty();
#endif

	for (Shader& shader : shadersList)
		shadersToCompile.pushBack(&shader);

	CompilerDXC compiler;

	// Compilation
	for (const Shader* shader : shadersToCompile)
	{
		compiler.compile(shader->getShaderType(), shader->getSourceMain(), sourcesCache);
	}

	return 0;
}