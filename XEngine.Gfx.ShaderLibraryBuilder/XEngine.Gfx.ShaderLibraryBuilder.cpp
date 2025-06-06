#include <algorithm>

#include <XLib.h>
#include <XLib.CharStream.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.CRC.h>
#include <XLib.FileSystem.h>
#include <XLib.Fmt.h>
#include <XLib.Path.h>
#include <XLib.String.h>
#include <XLib.System.Environment.h>
#include <XLib.System.File.h>

#include <XEngine.Gfx.ShaderLibraryFormat.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.BuildDepsTrace.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.Library.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.LibraryManifestLoader.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.SourceFileCache.h"
#include "XEngine.Utils.CmdLineArgsParser.h"

using namespace XLib;
using namespace XEngine::Gfx;
using namespace XEngine::Gfx::ShaderLibraryBuilder;
using namespace XEngine::Utils;

class Program
{
private:
	struct CmdArgs
	{
		InplaceStringASCIIx1024 libraryManifestFilePath;
		InplaceStringASCIIx1024 libraryFilePath;
		InplaceStringASCIIx1024 buildCacheDirPath;
	};

	enum class PrevBuildStatus : uint8
	{
		Outdated = 0,
		UpToDate,
	};

	static constexpr StringViewASCII BuildDepsTraceFileName = StringViewASCII::FromCStr("_BuildDepsTrace");
	static constexpr uint32 BuildDepsTraceFileMagic = 0x50E3D6BD; // Change this number to invalidate build cache after changing the compiler.

private:
	CmdArgs cmdArgs;
	StringViewASCII libraryRootPath;
	SourceFileCache sourceFileCache;

	Library library;

	BuildDepsTrace::Reader prevBuildDepsTrace;
	ArrayList<SourceFileHandle> prevBuildSourceFiles; // Source files mentioned in previous build's BuildDepsTrace file.

	BuildDepsTrace::Writer currentBuildDepsTrace;

private:
	bool parseCmdArgs();
	PrevBuildStatus loadPrevBuildInfo();
	void startWritingBuildDepsTrace();
	void loadCachedShaders();
	bool compileShaders();
	bool storeShaderLibrary();
	void finishWritingBuildDepsTrace(bool isBuildSuccessful);

	void composeShaderCompilationArtifactFilePath(VirtualStringRefASCII result, const Shader& shader, const char* filenameSuffix) const;
	void storeShaderCompilationArtifactToBuildCache(const Shader& shader, const HAL::ShaderCompiler::Blob& blob, const char* filenameSuffix) const;
	void storeShaderCompilationArtifactsToBuildCache(const Shader& shader, const HAL::ShaderCompiler::ShaderCompilationResult& compilationResult) const;

	static HAL::ShaderCompiler::BlobRef LoadShaderCompilerBlobFromFile(const char* pathCStr);

public:
	Program() = default;
	~Program() = default;

	int main();
};


////////////////////////////////////////////////////////////////////////////////////////////////////

bool Program::parseCmdArgs()
{
	static constexpr StringViewASCII LibraryManifestFilePathArgKey = StringViewASCII::FromCStr("--manifest");
	static constexpr StringViewASCII LibraryFilePathArgKey = StringViewASCII::FromCStr("--out");
	static constexpr StringViewASCII BuildCacheDirPathArgKey = StringViewASCII::FromCStr("--cache");

	StringViewASCII libraryManifestFilePathArgValue;
	StringViewASCII libraryFilePathArgValue;
	StringViewASCII buildCacheDirPathArgValue;

	const char* cmdLine = Environment::GetCommandLineCStr();
	CmdLineArgsParser parser(cmdLine);

	// Skip first argument.
	if (!parser.advance())
		return false;

	for (;;)
	{
		if (!parser.advance())
			return false;

		if (parser.getCurrentArgType() == CmdLineArgType::None)
			break;

		bool invalidArg = false;
		if (parser.getCurrentArgType() == CmdLineArgType::KeyValuePair)
		{
			if (parser.getCurrentArgKey() == LibraryManifestFilePathArgKey)
				libraryManifestFilePathArgValue = parser.getCurrentArgValue();
			else if (parser.getCurrentArgKey() == LibraryFilePathArgKey)
				libraryFilePathArgValue = parser.getCurrentArgValue();
			else if (parser.getCurrentArgKey() == BuildCacheDirPathArgKey)
				buildCacheDirPathArgValue = parser.getCurrentArgValue();
			else
				invalidArg = true;
		}
		else
			invalidArg = true;

		if (invalidArg)
		{
			if (parser.getCurrentArgType() == CmdLineArgType::Key || parser.getCurrentArgType() == CmdLineArgType::KeyValuePair)
				FmtPrintStdOut("error: invalid command line argument key '", parser.getCurrentArgKey(), "'\n");
			else
				FmtPrintStdOut("error: invalid command line argument '", parser.getCurrentArgRawString(), "'\n");
			return false;
		}
	}

	// TODO: Check that all pathes are valid.

	if (libraryManifestFilePathArgValue.isEmpty())
	{
		FmtPrintStdOut("error: missing library manifest file path. Use '", LibraryManifestFilePathArgKey, "=XXX'\n");
		return false;
	}
	if (libraryFilePathArgValue.isEmpty())
	{
		FmtPrintStdOut("error: missing output library file path. Use '", LibraryFilePathArgKey, "=XXX'\n");
		return false;
	}
	if (buildCacheDirPathArgValue.isEmpty())
	{
		FmtPrintStdOut("warning: missing build cache dir path. Use '", BuildCacheDirPathArgKey, "=XXX'. Incremental building is disabled\n");
	}

	Path::MakeAbsolute(libraryManifestFilePathArgValue, cmdArgs.libraryManifestFilePath);
	Path::MakeAbsolute(libraryFilePathArgValue, cmdArgs.libraryFilePath);
	Path::MakeAbsolute(buildCacheDirPathArgValue, cmdArgs.buildCacheDirPath);
	Path::AddTrailingDirectorySeparator(cmdArgs.buildCacheDirPath);

	XAssert(!cmdArgs.libraryManifestFilePath.isFull());
	XAssert(!cmdArgs.libraryFilePath.isFull());
	XAssert(!cmdArgs.buildCacheDirPath.isFull());

	if (!Path::HasFileName(cmdArgs.libraryManifestFilePath))
	{
		FmtPrintStdOut("error: library manifest file path has no filename\n");
		return false;
	}
	if (!Path::HasFileName(cmdArgs.libraryFilePath))
	{
		FmtPrintStdOut("error: output library file path has no filename\n");
		return false;
	}

	return true;
}

auto Program::loadPrevBuildInfo() -> PrevBuildStatus
{
	if (cmdArgs.buildCacheDirPath.isEmpty())
		return PrevBuildStatus::Outdated;

	InplaceStringASCIIx1024 buildDepsTraceFilePath;
	buildDepsTraceFilePath.append(cmdArgs.buildCacheDirPath);
	buildDepsTraceFilePath.append(BuildDepsTraceFileName);
	XAssert(!buildDepsTraceFilePath.isFull());

	prevBuildDepsTrace.load(buildDepsTraceFilePath.getCStr(), BuildDepsTraceFileMagic);
	if (!prevBuildDepsTrace.isLoaded())
	{
		FmtPrintStdOut("BuildDepsTrace file was not loaded. Incremental build is disabled\n");
		return PrevBuildStatus::Outdated;
	}

	// NOTE: If we can't retrieve mod time for a file, we consider it to be invalidated.

	const uint64 manifestFileModTime = FileSystem::GetFileModificationTime(cmdArgs.libraryManifestFilePath.getCStr());
	const bool manifiestFileIsOutdated =
		!StringsASCII::IsEqualIgnoreCase(cmdArgs.libraryManifestFilePath, prevBuildDepsTrace.getManifestFilePath()) ||
		manifestFileModTime != prevBuildDepsTrace.getManifestFileModTime() ||
		manifestFileModTime == InvalidTimePoint;

	bool someSourceFilesAreOutdated = false;
	prevBuildSourceFiles.resize(prevBuildDepsTrace.getSourceFileCount());
	for (uint16 i = 0; i < prevBuildDepsTrace.getSourceFileCount(); i++)
	{
		const SourceFileHandle sourceFile = sourceFileCache.openFile(prevBuildDepsTrace.getSourceFilePath(i));
		prevBuildSourceFiles[i] = sourceFile;

		bool fileIsOutdated = false;
		if (sourceFile == SourceFileHandle(0))
			fileIsOutdated = true;
		else if (sourceFileCache.getFileModTime(sourceFile) != prevBuildDepsTrace.getSourceFileModTime(i))
			fileIsOutdated = true;

		someSourceFilesAreOutdated |= fileIsOutdated;
	}

	bool libraryFileNeedsToBeRebuilt = false;
	if (prevBuildDepsTrace.wasBuildSuccessful())
	{
		const uint64 libraryFileModTime = FileSystem::GetFileModificationTime(cmdArgs.libraryFilePath.getCStr());
		libraryFileNeedsToBeRebuilt =
			!StringsASCII::IsEqualIgnoreCase(cmdArgs.libraryFilePath, prevBuildDepsTrace.getBuiltLibraryFilePath()) ||
			libraryFileModTime != prevBuildDepsTrace.getBuiltLibraryFileModTime() ||
			libraryFileModTime == InvalidTimePoint;
	}
	else
		libraryFileNeedsToBeRebuilt = true;

	if (!manifiestFileIsOutdated && !someSourceFilesAreOutdated && !libraryFileNeedsToBeRebuilt)
	{
		FmtPrintStdOut("Shader library '", cmdArgs.libraryFilePath, "' is up to date\n");
		return PrevBuildStatus::UpToDate;
	}

	return PrevBuildStatus::Outdated;
}

void Program::startWritingBuildDepsTrace()
{
	if (cmdArgs.buildCacheDirPath.isEmpty())
		return;

	InplaceStringASCIIx1024 buildDepsTraceFilePath;
	buildDepsTraceFilePath.append(cmdArgs.buildCacheDirPath);
	buildDepsTraceFilePath.append(BuildDepsTraceFileName);
	XAssert(!buildDepsTraceFilePath.isFull());

	// NOTE: Even if mod time is invalid, we still use it. This will simply cause the manifest file to be treated as invalidated during the next build.
	const uint64 manifestFileModTime = FileSystem::GetFileModificationTime(cmdArgs.libraryManifestFilePath.getCStr());
	currentBuildDepsTrace.openForWriting(buildDepsTraceFilePath.getCStr(), BuildDepsTraceFileMagic,
		cmdArgs.libraryManifestFilePath, manifestFileModTime, sourceFileCache);
}

void Program::loadCachedShaders()
{
	if (!prevBuildDepsTrace.isLoaded())
		return;

	XAssert(prevBuildDepsTrace.getSourceFileCount() == prevBuildSourceFiles.getSize());

	uint32 cachedShaderCount = 0;
	ArrayList<SourceFileHandle> shaderSourceFiles;

	for (uint32 libraryShaderIndex = 0; libraryShaderIndex < library.shaders.getSize(); libraryShaderIndex++)
	{
		// Check if the compilation parameters match and all files are up-to-date
		// If so, attempt to load the compiled shader from the build cache.

		Shader& shader = *library.shaders[libraryShaderIndex].get();

		const uint16 bdtShaderIndex = prevBuildDepsTrace.findShader(shader.getNameXSH());
		if (bdtShaderIndex == uint16(-1))
			continue;

		const bool shaderDetailsMatch = prevBuildDepsTrace.doShaderDetailsMatch(bdtShaderIndex,
			shader.getPipelineLayoutNameXSH(), shader.getPipelineLayout().getSourceHash(), shader.getCompilationArgs());
		if (!shaderDetailsMatch)
			continue;

		// Check if main source file matches.
		bool mainSourceFileMatches = false;
		{
			const uint32 bdtShaderMainSourceFileIndex = prevBuildDepsTrace.getShaderSourceFileIndex(bdtShaderIndex, 0);
			const SourceFileHandle bdtShaderMainSourceFile = prevBuildSourceFiles[bdtShaderMainSourceFileIndex];

			InplaceStringASCIIx1024 mainSourceFilePath;
			mainSourceFilePath.append(libraryRootPath);
			mainSourceFilePath.append(shader.getMainSourceFilePath());
			const SourceFileHandle shaderMainSourceFile = sourceFileCache.openFile(mainSourceFilePath);

			mainSourceFileMatches = (shaderMainSourceFile == bdtShaderMainSourceFile);
		}
		if (!mainSourceFileMatches)
			continue;

		// Check if some source files are outdated.
		bool sourceFilesAreUpToDate = true;
		if (prevBuildDepsTrace.getShaderSourceFileCount(bdtShaderIndex) == 0)
			sourceFilesAreUpToDate = false;
		else
		{
			shaderSourceFiles.clear();
			for (uint16 i = 0; i < prevBuildDepsTrace.getShaderSourceFileCount(bdtShaderIndex); i++)
			{
				const uint16 bdtSourceFileIndex = prevBuildDepsTrace.getShaderSourceFileIndex(bdtShaderIndex, i);
				XAssert(bdtSourceFileIndex < prevBuildSourceFiles.getSize());
				const SourceFileHandle sourceFile = prevBuildSourceFiles[bdtSourceFileIndex];
				shaderSourceFiles.pushBack(sourceFile);

				if (sourceFile == SourceFileHandle(0) ||
					sourceFileCache.getFileModTime(sourceFile) != prevBuildDepsTrace.getSourceFileModTime(bdtSourceFileIndex))
				{
					sourceFilesAreUpToDate = false;
					break;
				}
			}
		}
		if (!sourceFilesAreUpToDate)
			continue;

		InplaceStringASCIIx1024 compiledBlobFilePath;
		composeShaderCompilationArtifactFilePath(compiledBlobFilePath, shader, ".bin");

		// Check if compiled blob modification time matches.
		const uint64 compiledBlobModTime = FileSystem::GetFileModificationTime(compiledBlobFilePath.getCStr());
		const bool compiledBlobModTimeMatches =
			compiledBlobModTime != InvalidTimePoint &&
			compiledBlobModTime == prevBuildDepsTrace.getShaderCompiledBlobModTime(bdtShaderIndex);
		if (!compiledBlobModTimeMatches)
			continue;

		// Shader fully matches the one stored in build cache. Try load compiled blob from build cache.
		const HAL::ShaderCompiler::BlobRef blob = LoadShaderCompilerBlobFromFile(compiledBlobFilePath.getCStr());
		if (!blob)
			continue;
		shader.setCompiledBlob(blob.get());

		currentBuildDepsTrace.addShader(shader.getNameXSH(),
			shader.getPipelineLayoutNameXSH(), shader.getPipelineLayout().getSourceHash(), shader.getCompilationArgs(),
			shaderSourceFiles, XCheckedCastU16(shaderSourceFiles.getSize()), compiledBlobModTime);
		cachedShaderCount++;
	}

	FmtPrintStdOut("Loaded ", cachedShaderCount, '/', library.shaders.getSize(), " shaders from build cache\n");
}

bool Program::compileShaders()
{
	ArrayList<Shader*> shadersToCompile;
	for (uint32 i = 0; i < library.shaders.getSize(); i++)
	{
		if (!library.shaders[i]->getCompiledBlob())
			shadersToCompile.pushBack(library.shaders[i].get());
	}

	if (!shadersToCompile.getSize())
		return true;

	FmtPrintStdOut("Compiling ", shadersToCompile.getSize(), " shaders\n");

	// Sort shaders by name to make the log look nice :sparkles:
	std::sort(shadersToCompile.begin(), shadersToCompile.end(),
		[](const Shader* left, const Shader* right) -> bool { return String::IsLess(left->getName(), right->getName()); });

	struct IncludeResolverContext
	{
		SourceFileCache& sourceFileCache;
		ArrayList<SourceFileHandle>& shaderSourceFiles;
	};

	auto resolveInclude = [](void* voidContext, StringViewASCII includeFilePath) -> HAL::ShaderCompiler::IncludeResolutionResult
	{
		IncludeResolverContext* context = (IncludeResolverContext*)voidContext;

		const SourceFileHandle includeFile = context->sourceFileCache.openFile(includeFilePath);
		if (includeFile == SourceFileHandle(0))
			return HAL::ShaderCompiler::IncludeResolutionResult{ .status = false, };
		StringViewASCII includeFileText;
		if (!context->sourceFileCache.getFileText(includeFile, includeFileText))
			return HAL::ShaderCompiler::IncludeResolutionResult{ .status = false, };

		context->shaderSourceFiles.pushBack(includeFile);
		return HAL::ShaderCompiler::IncludeResolutionResult{ .text = includeFileText, .status = true, };
	};

	bool compilationSuccessful = true;
	ArrayList<SourceFileHandle> shaderSourceFiles;

	for (uint32 i = 0; i < shadersToCompile.getSize(); i++)
	{
		Shader& shader = *shadersToCompile[i];

		InplaceStringASCIIx256 messageHeader;
		FmtPrintStr(messageHeader, " [", i + 1, "/", shadersToCompile.getSize(), "] Compiling shader '", shader.getName(), '\'');
		FmtPrintStdOut(messageHeader, "\n");

		InplaceStringASCIIx1024 mainSourceFilePath;
		mainSourceFilePath.append(libraryRootPath);
		mainSourceFilePath.append(shader.getMainSourceFilePath());
		XAssert(!mainSourceFilePath.isFull());

		StringViewASCII mainSourceFileText;
		bool mainSourceFileTextLoaded = false;
		const SourceFileHandle mainSourceFile = sourceFileCache.openFile(mainSourceFilePath);
		if (mainSourceFile != SourceFileHandle(0))
		{
			if (sourceFileCache.getFileText(mainSourceFile, mainSourceFileText))
				mainSourceFileTextLoaded = true;
		}

		if (!mainSourceFileTextLoaded)
		{
			FmtPrintStdOut(messageHeader, ": error: failed to open file '", mainSourceFilePath, "'\n");
			compilationSuccessful = false;
			break;
		}

		shaderSourceFiles.clear();
		shaderSourceFiles.pushBack(mainSourceFile);

		IncludeResolverContext includeResolverContext = { .sourceFileCache = sourceFileCache, .shaderSourceFiles = shaderSourceFiles };

		const HAL::ShaderCompiler::ShaderCompilationResultRef compilationResult =
			HAL::ShaderCompiler::CompileShader(mainSourceFilePath, mainSourceFileText,
				shader.getCompilationArgs(), shader.getPipelineLayout(),
				resolveInclude, &includeResolverContext);

		compilationSuccessful = (compilationResult->getStatus() == HAL::ShaderCompiler::ShaderCompilationStatus::Success);

		if (!compilationSuccessful)
			FmtPrintStdOut(messageHeader, ": compilation failed\n");

		if (compilationResult->getPreprocessorStdOut().getLength() > 0)
			FmtPrintStdOut(compilationResult->getPreprocessorStdOut(), "\n");
		if (compilationResult->getCompilerStdOut().getLength() > 0)
			FmtPrintStdOut(compilationResult->getCompilerStdOut(), "\n");

		storeShaderCompilationArtifactsToBuildCache(shader, *compilationResult);

		if (!compilationSuccessful)
			break;

		shader.setCompiledBlob(compilationResult->getBytecodeBlob());

		uint64 compiledBlobModTime = InvalidTimePoint;
		{
			InplaceStringASCIIx1024 compiledBlobFilePath;
			composeShaderCompilationArtifactFilePath(compiledBlobFilePath, shader, ".bin");

			// NOTE: Even if mod time is invalid, we still use it. This will simply cause the shader to be treated as invalidated during the next build.
			// We can't skip putting this shader to the BuildDepsTrace, as it’s still a dependency that may invalidate the built library.
			compiledBlobModTime = FileSystem::GetFileModificationTime(compiledBlobFilePath.getCStr());
		}

		currentBuildDepsTrace.addShader(shader.getNameXSH(),
			shader.getPipelineLayoutNameXSH(), shader.getPipelineLayout().getSourceHash(), shader.getCompilationArgs(),
			shaderSourceFiles, XCheckedCastU16(shaderSourceFiles.getSize()), compiledBlobModTime);
	}

	return compilationSuccessful;
}

bool Program::storeShaderLibrary()
{
	FmtPrintStdOut("Storing shader library '", cmdArgs.libraryFilePath, "'\n");

	XTODO("Sort objects in order of XSH increase");

	ArrayList<ShaderLibraryFormat::DescriptorSetLayoutRecord> descriptorSetLayoutRecords;
	ArrayList<ShaderLibraryFormat::PipelineLayoutRecord> pipelineLayoutRecords;
	ArrayList<ShaderLibraryFormat::ShaderRecord> shaderRecords;

	descriptorSetLayoutRecords.resize(library.descriptorSetLayouts.getSize());
	pipelineLayoutRecords.resize(library.pipelineLayouts.getSize());
	shaderRecords.resize(library.shaders.getSize());

	memorySet(descriptorSetLayoutRecords.getData(), 0, descriptorSetLayoutRecords.getByteSize());
	memorySet(pipelineLayoutRecords.getData(), 0, pipelineLayoutRecords.getByteSize());
	memorySet(shaderRecords.getData(), 0, shaderRecords.getByteSize());

	struct BlobDataView
	{
		const void* data;
		uint32 size;
	};

	ArrayList<BlobDataView> blobs;
	blobs.reserve(
		library.descriptorSetLayouts.getSize() +
		library.pipelineLayouts.getSize() +
		library.shaders.getSize());

	uint32 blobsDataSizeAccum = 0;

	for (uint32 i = 0; i < library.descriptorSetLayouts.getSize(); i++)
	{
		const uint64 nameXSH = library.descriptorSetLayouts[i].nameXSH;
		const HAL::ShaderCompiler::DescriptorSetLayout& dsl = *library.descriptorSetLayouts[i].ref.get();
		const uint32 blobCRC32 = CRC32::Compute(dsl.getBlobData(), dsl.getBlobSize());

		ShaderLibraryFormat::DescriptorSetLayoutRecord& record = descriptorSetLayoutRecords[i];
		record.nameXSH0 = uint32(nameXSH);
		record.nameXSH1 = uint32(nameXSH >> 32);
		record.blobOffset = blobsDataSizeAccum;
		record.blobSize = dsl.getBlobSize();
		record.blobCRC32 = blobCRC32;

		blobs.pushBack(BlobDataView{ dsl.getBlobData(), dsl.getBlobSize() });
		blobsDataSizeAccum += dsl.getBlobSize();
	}

	for (uint32 i = 0; i < library.pipelineLayouts.getSize(); i++)
	{
		const uint64 nameXSH = library.pipelineLayouts[i].nameXSH;
		const HAL::ShaderCompiler::PipelineLayout& pipelineLayout = *library.pipelineLayouts[i].ref.get();
		const uint32 blobCRC32 = CRC32::Compute(pipelineLayout.getBlobData(), pipelineLayout.getBlobSize());

		ShaderLibraryFormat::PipelineLayoutRecord& record = pipelineLayoutRecords[i];
		record.nameXSH0 = uint32(nameXSH);
		record.nameXSH1 = uint32(nameXSH >> 32);
		record.blobOffset = blobsDataSizeAccum;
		record.blobSize = pipelineLayout.getBlobSize();
		record.blobCRC32 = blobCRC32;

		blobs.pushBack(BlobDataView{ pipelineLayout.getBlobData(), pipelineLayout.getBlobSize() });
		blobsDataSizeAccum += pipelineLayout.getBlobSize();
	}

	for (uint32 i = 0; i < library.shaders.getSize(); i++)
	{
		const Shader& shader = *library.shaders[i].get();
		const uint64 nameXSH = shader.getNameXSH();
		const HAL::ShaderCompiler::Blob* shaderBlob = shader.getCompiledBlob();
		XAssert(shaderBlob);
		const uint32 blobCRC32 = CRC32::Compute(shaderBlob->getData(), shaderBlob->getSize());

		ShaderLibraryFormat::ShaderRecord& record = shaderRecords[i];
		record.nameXSH0 = uint32(nameXSH);
		record.nameXSH1 = uint32(nameXSH >> 32);
		record.blobOffset = blobsDataSizeAccum;
		record.blobSize = shaderBlob->getSize();
		record.blobCRC32 = blobCRC32;
		record.pipelineLayoutNameXSH0 = uint32(shader.getPipelineLayoutNameXSH());
		record.pipelineLayoutNameXSH1 = uint32(shader.getPipelineLayoutNameXSH() >> 32);

		blobs.pushBack(BlobDataView{ shaderBlob->getData(), shaderBlob->getSize() });
		blobsDataSizeAccum += shaderBlob->getSize();
	}

	const uint32 blobsDataSize = blobsDataSizeAccum;

	const uintptr blobsDataOffset =
		sizeof(ShaderLibraryFormat::LibraryHeader) +
		descriptorSetLayoutRecords.getByteSize() +
		pipelineLayoutRecords.getByteSize() +
		shaderRecords.getByteSize();

	ShaderLibraryFormat::LibraryHeader header = {};
	header.signature = ShaderLibraryFormat::LibrarySignature;
	header.version = ShaderLibraryFormat::LibraryCurrentVersion;
	header.descriptorSetLayoutCount = XCheckedCastU16(descriptorSetLayoutRecords.getSize());
	header.pipelineLayoutCount = XCheckedCastU16(pipelineLayoutRecords.getSize());
	header.shaderCount = XCheckedCastU16(shaderRecords.getSize());
	header.blobsDataOffset = uint32(blobsDataOffset);
	header.blobsDataSize = blobsDataSize;

	FileSystem::CreateDirRecursive(Path::GetParent(cmdArgs.libraryFilePath.getCStr()));

	File file;
	file.open(cmdArgs.libraryFilePath.getCStr(), FileAccessMode::Write, FileOpenMode::Override);
	if (!file.isOpen())
	{
		FmtPrintStdOut("error: failed to open shader library for writing '", cmdArgs.libraryFilePath, "'\n");
		return false;
	}

	file.write(&header, sizeof(header));
	file.write(descriptorSetLayoutRecords.getData(), descriptorSetLayoutRecords.getByteSize());
	file.write(pipelineLayoutRecords.getData(), pipelineLayoutRecords.getByteSize());
	file.write(shaderRecords.getData(), shaderRecords.getByteSize());

	uint32 blobsDataSizeCheck = 0;
	for (const BlobDataView& blob : blobs)
	{
		file.write(blob.data, blob.size);
		blobsDataSizeCheck += blob.size;
	}
	XAssert(blobsDataSizeCheck == blobsDataSize);

	file.close();

	return true;
}

void Program::finishWritingBuildDepsTrace(bool isBuildSuccessful)
{
	if (isBuildSuccessful)
	{
		// NOTE: Even if mod time is invalid, we still use it. This will simply cause the library file to be treated as invalidated during the next build.
		const uint64 libraryFileModTime = FileSystem::GetFileModificationTime(cmdArgs.libraryFilePath.getCStr());
		currentBuildDepsTrace.closeAfterSuccessfulBuild(cmdArgs.libraryFilePath, libraryFileModTime);
	}
	else
		currentBuildDepsTrace.closeAfterFailedBuild();
}

void Program::composeShaderCompilationArtifactFilePath(VirtualStringRefASCII result, const Shader& shader, const char* filenameSuffix) const
{
	VirtualStringWriter resultWriter(result);
	FmtPrint(resultWriter, cmdArgs.buildCacheDirPath, shader.getName(), '#', FmtArgHex64(shader.getNameXSH(), 16), filenameSuffix);
}

void Program::storeShaderCompilationArtifactToBuildCache(const Shader& shader, const HAL::ShaderCompiler::Blob& blob, const char* filenameSuffix) const
{
	if (cmdArgs.buildCacheDirPath.isEmpty())
		return;

	InplaceStringASCIIx1024 filePath;
	composeShaderCompilationArtifactFilePath(filePath, shader, filenameSuffix);

	File file;
	file.open(filePath.getCStr(), FileAccessMode::Write, FileOpenMode::Override);
	if (!file.isOpen())
	{
		FmtPrintStdOut("warning: failed to open file '", filePath, "' for writing\n");
		return;
	}

	file.write(blob.getData(), blob.getSize());
	file.close();
}

void Program::storeShaderCompilationArtifactsToBuildCache(const Shader& shader, const HAL::ShaderCompiler::ShaderCompilationResult& compilationResult) const
{
	if (const HAL::ShaderCompiler::Blob* bytecodeBlob = compilationResult.getBytecodeBlob())
		storeShaderCompilationArtifactToBuildCache(shader, *bytecodeBlob, ".bin");

	if (const HAL::ShaderCompiler::Blob* preprocBlob = compilationResult.getPreprocessedSourceBlob())
		storeShaderCompilationArtifactToBuildCache(shader, *preprocBlob, ".preproc.hlsl");
}

HAL::ShaderCompiler::BlobRef Program::LoadShaderCompilerBlobFromFile(const char* pathCStr)
{
	File file;
	file.open(pathCStr, FileAccessMode::Read, FileOpenMode::OpenExisting);
	if (!file.isOpen())
		return nullptr;

	const uint64 fileSize = file.getSize();
	if (fileSize == uint64(-1))
		return nullptr;
	if (fileSize > uint64(uint32(-1)))
		return nullptr;

	const HAL::ShaderCompiler::BlobRef blob = HAL::ShaderCompiler::Blob::Create(uint32(fileSize));
	if (!file.read((void*)blob->getData(), fileSize))
		return nullptr;

	return blob;
}

int Program::main()
{
	if (!parseCmdArgs())
		return 1;

	libraryRootPath = Path::GetParent(cmdArgs.libraryManifestFilePath);
	XAssert(Path::HasTrailingDirectorySeparator(libraryRootPath));
	FileSystem::CreateDirRecursive(cmdArgs.buildCacheDirPath);

	const PrevBuildStatus prevBuildStatus = loadPrevBuildInfo();
	if (prevBuildStatus == PrevBuildStatus::UpToDate)
		return 0;

	FmtPrintStdOut("Loading shader library manifest file '", cmdArgs.libraryManifestFilePath, "'\n");
	if (!LibraryManifestLoader::Load(library, cmdArgs.libraryManifestFilePath.getCStr()))
		return 1;

	if (!library.shaders.getSize())
	{
		FmtPrintStdOut("error: no shaders declared in library manifest file\n");
		return 1;
	}

	FmtPrintStdOut("Shader library manifest declares ",
		library.shaders.getSize(), " shaders, ",
		library.pipelineLayouts.getSize(), " pipeline layouts, ",
		library.descriptorSetLayouts.getSize(), " descriptor set layouts\n");

	startWritingBuildDepsTrace();

	loadCachedShaders();

	bool isBuildSuccessful = false;
	if (compileShaders())
		if (storeShaderLibrary())
			isBuildSuccessful = true;

	finishWritingBuildDepsTrace(isBuildSuccessful);

	if (isBuildSuccessful)
		FmtPrintStdOut("Shader library '", cmdArgs.libraryFilePath, "' was built succesfully\n");
	else
		FmtPrintStdOut("Shader library '", cmdArgs.libraryFilePath, "' build failed\n");

	return isBuildSuccessful ? 0 : 1;
}


////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
	Program program;
	return program.main();
}
