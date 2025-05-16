#include <XLib.h>
#include <XLib.Algorithm.QuickSort.h>
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

#include "XEngine.Gfx.ShaderLibraryBuilder.Library.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.LibraryManifestLoader.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.SourceCache.h"
#include "XEngine.Utils.CmdLineArgsParser.h"

// TODO: Implement `FileSystem::CreateDirs()` and create output and intermediate dirs.

using namespace XLib;
using namespace XEngine::Gfx;
using namespace XEngine::Gfx::ShaderLibraryBuilder;
using namespace XEngine::Utils;

struct CmdArgs
{
	InplaceStringASCIIx1024 libraryManifestFilePath;
	InplaceStringASCIIx1024 outLibraryFilePath;
	InplaceStringASCIIx1024 buildCacheDirPath;
};

static bool ParseCmdArgs(CmdArgs& resultCmdArgs)
{
	static constexpr StringViewASCII LibraryManifestFilePathArgKey = StringViewASCII::FromCStr("--manifest");
	static constexpr StringViewASCII OutLibraryFilePathArgKey = StringViewASCII::FromCStr("--out");
	static constexpr StringViewASCII BuildCacheDirPathArgKey = StringViewASCII::FromCStr("--cache");

	StringViewASCII libraryManifestFilePathArgValue;
	StringViewASCII outLibraryFilePathArgValue;
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
			else if (parser.getCurrentArgKey() == OutLibraryFilePathArgKey)
				outLibraryFilePathArgValue = parser.getCurrentArgValue();
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
	if (outLibraryFilePathArgValue.isEmpty())
	{
		FmtPrintStdOut("error: missing output library file path. Use '", OutLibraryFilePathArgKey, "=XXX'\n");
		return false;
	}
	if (buildCacheDirPathArgValue.isEmpty())
	{
		FmtPrintStdOut("warning: missing build cache dir path. Use '", BuildCacheDirPathArgKey, "=XXX'. Incremental building is disabled\n");
	}

	Path::MakeAbsolute(libraryManifestFilePathArgValue, resultCmdArgs.libraryManifestFilePath);
	Path::MakeAbsolute(outLibraryFilePathArgValue, resultCmdArgs.outLibraryFilePath);
	Path::MakeAbsolute(buildCacheDirPathArgValue, resultCmdArgs.buildCacheDirPath);
	Path::AddTrailingDirectorySeparator(resultCmdArgs.buildCacheDirPath);

	if (!Path::HasFileName(resultCmdArgs.libraryManifestFilePath))
	{
		FmtPrintStdOut("error: library manifest file path has no filename\n");
		return false;
	}
	if (!Path::HasFileName(resultCmdArgs.outLibraryFilePath))
	{
		FmtPrintStdOut("error: output library file path has no filename\n");
		return false;
	}

	return true;
}

static void LoadBuildCache(const CmdArgs& cmdArgs, Library& library)
{

}

static void StoreSingleShaderCompilationArtifactToBuildCache(const CmdArgs& cmdArgs, const Shader& shader,
	const HAL::ShaderCompiler::Blob& blob, const char* filenameSuffix)
{
	if (cmdArgs.buildCacheDirPath.isEmpty())
		return;

	InplaceStringASCIIx1024 filePath;
	FmtPrintStr(filePath, cmdArgs.buildCacheDirPath,
		shader.getName(), '#', FmtArgHex64(shader.getNameXSH(), 16), filenameSuffix);

	File file;
	file.open(filePath.getCStr(), FileAccessMode::Write, FileOpenMode::Override);
	if (!file.isOpen())
	{
		FmtPrintStdOut("warning: Cannot open file '", filePath, "' for writing\n");
		return;
	}

	file.write(blob.getData(), blob.getSize());
	file.close();
}

static void StoreShaderCompilationArtifactsToBuildCache(const CmdArgs& cmdArgs,
	const Shader& shader, const HAL::ShaderCompiler::ShaderCompilationResult& compilationResult)
{
	if (const HAL::ShaderCompiler::Blob* bytecodeBlob = compilationResult.getBytecodeBlob())
		StoreSingleShaderCompilationArtifactToBuildCache(cmdArgs, shader, *bytecodeBlob, ".bin");

	if (const HAL::ShaderCompiler::Blob* preprocBlob = compilationResult.getPreprocessedSourceBlob())
		StoreSingleShaderCompilationArtifactToBuildCache(cmdArgs, shader, *preprocBlob, ".preproc.hlsl");
}

static bool StoreShaderLibrary(const CmdArgs& cmdArgs, const Library& library)
{
	FmtPrintStdOut("Storing shader library '", cmdArgs.outLibraryFilePath, "'\n");

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

	FileSystem::CreateDirRecursive(Path::GetParent(cmdArgs.outLibraryFilePath.getCStr()));

	File file;
	file.open(cmdArgs.outLibraryFilePath.getCStr(), FileAccessMode::Write, FileOpenMode::Override);
	if (!file.isOpen())
	{
		FmtPrintStdOut("error: Cannot open shader library for writing '", cmdArgs.outLibraryFilePath, "'\n");
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

static void StoreBuildCacheIndex(const CmdArgs& cmdArgs, const Library& library)
{
	if (cmdArgs.buildCacheDirPath.isEmpty())
		return;

	InplaceStringASCIIx1024 filePath;
	filePath.append(cmdArgs.buildCacheDirPath);
	filePath.append("_BuildCacheIndex.txt");

	FileCharStreamWriter fileWriter;
	fileWriter.open(filePath.getCStr(), true);
	if (!fileWriter.isOpen())
	{
		FmtPrintStdOut("warning: Cannot open file '", filePath, "' for writing\n");
		return;
	}

	auto ShaderTypeToString = [](HAL::ShaderType type) -> const char*
	{
		switch (type)
		{
			case HAL::ShaderType::Compute:			return "CS";
			case HAL::ShaderType::Vertex:			return "VS";
			case HAL::ShaderType::Amplification:	return "AS";
			case HAL::ShaderType::Mesh:				return "MS";
			case HAL::ShaderType::Pixel:			return "PS";
		}
		return nullptr;
	};

	for (const ShaderRef& shader : library.shaders)
	{
		FmtPrint(fileWriter,
			"SH:", shader->getName(), '#', FmtArgHex64(shader->getNameXSH(), 16), '/',
			"T:", ShaderTypeToString(shader->getCompilationArgs().shaderType), '/',
			"PL:", shader->getPipelineLayoutName(), '#', FmtArgHex64(shader->getPipelineLayoutNameXSH(), 16), '/',
			"PLHASH:", FmtArgHex64(shader->getPipelineLayout().getSourceHash(), 16), '/',
			"EP:", shader->getCompilationArgs().entryPointName, '\n');
	}

	fileWriter.close();
}

static HAL::ShaderCompiler::SourceResolutionResult ResolveSource(void* context, StringViewASCII sourceFilename)
{
	SourceCache& sourceCache = *(SourceCache*)context;
	HAL::ShaderCompiler::SourceResolutionResult result = {};
	result.resolved = sourceCache.resolve(sourceFilename, result.text);
	return result;
}

int main()
{
	CmdArgs cmdArgs;
	if (!ParseCmdArgs(cmdArgs))
		return 1;

	// Load library manifest file.
	Library library;
	FmtPrintStdOut("Loading shader library manifest file '", cmdArgs.libraryManifestFilePath, "'\n");
	if (!LibraryManifestLoader::Load(library, cmdArgs.libraryManifestFilePath.getCStr()))
		return 1;

	if (!library.shaders.getSize())
	{
		FmtPrintStdOut("No shaders declared in library manifest file. No shader library produced\n");
		return 1;
	}

	LoadBuildCache(cmdArgs, library);

	// Gather shaders that were not loaded from build cache and need to be compiled.
	ArrayList<Shader*> shadersToCompile;
	for (const ShaderRef& shader : library.shaders)
	{
		if (!shader->getCompiledBlob())
			shadersToCompile.pushBack(shader.get());
	}

#if 0
	if (!shadersToCompile.getSize())
	{
		// All shaders are loaded from build cache.
		// Check if we need to re-link shader library file.
	}
#endif

	// Sort shaders by name to make the log look nice :sparkles:
	QuickSort<Shader*>(shadersToCompile, shadersToCompile.getSize(),
		[](const Shader* left, const Shader* right) -> bool { return String::IsLess(left->getName(), right->getName()); });

	InplaceStringASCIIx1024 librarySourceRootPath = Path::GetParent(cmdArgs.libraryManifestFilePath);
	XAssert(Path::HasTrailingDirectorySeparator(librarySourceRootPath));

	SourceCache sourceCache;

	// Actual compilation.
	for (uint32 i = 0; i < shadersToCompile.getSize(); i++)
	{
		Shader& shader = *shadersToCompile[i];

		InplaceStringASCIIx256 messageHeader;
		FmtPrintStr(messageHeader, " [", i + 1, "/", shadersToCompile.getSize(), "] Compiling shader '", shader.getName(), '\'');
		FmtPrintStdOut(messageHeader, "\n");

		InplaceStringASCIIx1024 mainSourceFilePath;
		mainSourceFilePath.append(librarySourceRootPath);
		mainSourceFilePath.append(shader.getMainSourceFilename());
		// TODO: At this point `mainSourceFilePath` should be normalized (and it is not guaranteed to be normalized right now).
		// We either need to do normalization here or we guarantee that shader `mainSourceFilename` is relative and does not
		// point upward.

		HAL::ShaderCompiler::ShaderCompilationResultRef compilationResult =
			HAL::ShaderCompiler::CompileShader(
				mainSourceFilePath, shader.getCompilationArgs(), shader.getPipelineLayout(),
				&ResolveSource, &sourceCache);

		const bool compilationFailed = compilationResult->getStatus() != HAL::ShaderCompiler::ShaderCompilationStatus::Success;

		if (compilationFailed)
			FmtPrintStdOut(messageHeader, ": compilation failed\n");

		if (compilationResult->getPreprocessingOuput().getLength() > 0)
			FmtPrintStdOut(compilationResult->getPreprocessingOuput(), "\n");
		if (compilationResult->getCompilationOutput().getLength() > 0)
			FmtPrintStdOut(compilationResult->getCompilationOutput(), "\n");

		StoreShaderCompilationArtifactsToBuildCache(cmdArgs, shader, *compilationResult);

		if (compilationFailed)
			return 1;

		shader.setCompiledBlob(compilationResult->getBytecodeBlob());
	}

	if (!StoreShaderLibrary(cmdArgs, library))
		return 1;

	StoreBuildCacheIndex(cmdArgs, library);

	FmtPrintStdOut("Shader library build succeeded\n");

	return 0;
}
