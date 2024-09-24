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

#include "XEngine.Gfx.ShaderLibraryBuilder.LibraryDefinition.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.LibraryDefinitionLoader.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.SourceCache.h"
#include "XEngine.Utils.CmdLineArgsParser.h"

// TODO: Implement `FileSystem::CreateDirs()` and create output and intermediate dirs.

using namespace XLib;
using namespace XEngine::Gfx;
using namespace XEngine::Gfx::ShaderLibraryBuilder;
using namespace XEngine::Utils;

struct CmdArgs
{
	InplaceStringASCIIx1024 libraryDefinitionFilePath;
	InplaceStringASCIIx1024 outLibraryFilePath;
	InplaceStringASCIIx1024 buildCacheDirPath;
};

static bool ParseCmdArgs(CmdArgs& resultCmdArgs)
{
	static constexpr StringViewASCII LibraryDefinitionFilePathArgKey = StringViewASCII::FromCStr("--libdef");
	static constexpr StringViewASCII OutLibraryFilePathArgKey = StringViewASCII::FromCStr("--out");
	static constexpr StringViewASCII BuildCacheDirPathArgKey = StringViewASCII::FromCStr("--cache");

	StringViewASCII libraryDefinitionFilePathArgValue;
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

		if (parser.getCurentArgType() == CmdLineArgType::None)
			break;

		bool invalidArg = false;
		if (parser.getCurentArgType() == CmdLineArgType::KeyValuePair)
		{
			if (parser.getCurrentArgKey() == LibraryDefinitionFilePathArgKey)
				libraryDefinitionFilePathArgValue = parser.getCurrentArgValue();
			else if (parser.getCurrentArgKey() == OutLibraryFilePathArgKey)
				outLibraryFilePathArgValue = parser.getCurrentArgValue();
			else if (parser.getCurrentArgKey() == BuildCacheDirPathArgKey)
				buildCacheDirPathArgValue = parser.getCurrentArgValue();
			else
			{
				FmtPrintStdOut("error: invalid command line argument key '", parser.getCurrentArgKey(), "'\n");
				return false;
			}
		}
		else
		{
			FmtPrintStdOut("error: invalid command line argument '", parser.getCurrentArgRawString(), "'\n");
			return false;
		}
	}

	// TODO: Check that all pathes are valid.

	if (libraryDefinitionFilePathArgValue.isEmpty())
	{
		FmtPrintStdOut("error: missing library definition file path. Use '", LibraryDefinitionFilePathArgKey, "=XXX'\n");
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

	Path::MakeAbsolute(libraryDefinitionFilePathArgValue, resultCmdArgs.libraryDefinitionFilePath);
	Path::Normalize(resultCmdArgs.libraryDefinitionFilePath);

	Path::MakeAbsolute(outLibraryFilePathArgValue, resultCmdArgs.outLibraryFilePath);
	Path::Normalize(resultCmdArgs.outLibraryFilePath);

	Path::MakeAbsolute(buildCacheDirPathArgValue, resultCmdArgs.buildCacheDirPath);
	Path::Normalize(resultCmdArgs.buildCacheDirPath);
	Path::AddTrailingDirectorySeparator(resultCmdArgs.buildCacheDirPath);

	if (!Path::HasFileName(resultCmdArgs.libraryDefinitionFilePath))
	{
		FmtPrintStdOut("error: library definition file path has no filename\n");
		return false;
	}
	if (!Path::HasFileName(resultCmdArgs.outLibraryFilePath))
	{
		FmtPrintStdOut("error: output library file path has no filename\n");
		return false;
	}

	return true;
}

static void StoreSingleShaderCompilationArtifactToBuildCache(const CmdArgs& cmdArgs, const Shader& shader,
	const HAL::ShaderCompiler::Blob& blob, const char* filenameSuffix)
{
	if (cmdArgs.buildCacheDirPath.isEmpty())
		return;

	InplaceStringASCIIx1024 filePath;
	FmtPrintStr(filePath, cmdArgs.buildCacheDirPath,
		shader.getName(), "__[", FmtArgHex64(shader.getNameXSH(), 16), ']', filenameSuffix);

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

static bool StoreShaderLibrary(const CmdArgs& cmdArgs, const LibraryDefinition& libraryDefinition)
{
	FmtPrintStdOut("Storing shader library '", cmdArgs.outLibraryFilePath, "'\n");

	XTODO("Sort objects in order of XSH increase");

	ArrayList<ShaderLibraryFormat::DescriptorSetLayoutRecord> descriptorSetLayoutRecords;
	ArrayList<ShaderLibraryFormat::PipelineLayoutRecord> pipelineLayoutRecords;
	ArrayList<ShaderLibraryFormat::ShaderRecord> shaderRecords;

	descriptorSetLayoutRecords.resize(libraryDefinition.descriptorSetLayouts.getSize());
	pipelineLayoutRecords.resize(libraryDefinition.pipelineLayouts.getSize());
	shaderRecords.resize(libraryDefinition.shaders.getSize());

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
		libraryDefinition.descriptorSetLayouts.getSize() +
		libraryDefinition.pipelineLayouts.getSize() +
		libraryDefinition.shaders.getSize());

	uint32 blobsDataSizeAccum = 0;

	for (uint32 i = 0; i < libraryDefinition.descriptorSetLayouts.getSize(); i++)
	{
		const uint64 nameXSH = libraryDefinition.descriptorSetLayouts[i].nameXSH;
		const HAL::ShaderCompiler::DescriptorSetLayout& dsl = *libraryDefinition.descriptorSetLayouts[i].ref.get();
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

	for (uint32 i = 0; i < libraryDefinition.pipelineLayouts.getSize(); i++)
	{
		const uint64 nameXSH = libraryDefinition.pipelineLayouts[i].nameXSH;
		const HAL::ShaderCompiler::PipelineLayout& pipelineLayout = *libraryDefinition.pipelineLayouts[i].ref.get();
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

	for (uint32 i = 0; i < libraryDefinition.shaders.getSize(); i++)
	{
		const Shader& shader = *libraryDefinition.shaders[i].get();
		const uint64 nameXSH = shader.getNameXSH();
		const HAL::ShaderCompiler::Blob& shaderBlob = shader.getCompiledBlob();
		const uint32 blobCRC32 = CRC32::Compute(shaderBlob.getData(), shaderBlob.getSize());

		ShaderLibraryFormat::ShaderRecord& record = shaderRecords[i];
		record.nameXSH0 = uint32(nameXSH);
		record.nameXSH1 = uint32(nameXSH >> 32);
		record.blobOffset = blobsDataSizeAccum;
		record.blobSize = shaderBlob.getSize();
		record.blobCRC32 = blobCRC32;
		record.pipelineLayoutNameXSH0 = uint32(shader.getPipelineLayoutNameXSH());
		record.pipelineLayoutNameXSH1 = uint32(shader.getPipelineLayoutNameXSH() >> 32);

		blobs.pushBack(BlobDataView{ shaderBlob.getData(), shaderBlob.getSize() });
		blobsDataSizeAccum += shaderBlob.getSize();
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

	// TODO:
#if 0
	FileSystem::CreateDirs(Path::RemoveFileName(cmdArgs.outLibraryFilePath));
#endif

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

static void StoreBuildCacheIndex(const CmdArgs& cmdArgs, const LibraryDefinition& libraryDefinition)
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

	for (const ShaderRef& shader : libraryDefinition.shaders)
	{
		const uint64 shaderNameXSH = shader->getNameXSH();
		const StringViewASCII shaderName = shader->getName();
		const StringViewASCII shaderEntryPointName = shader->getCompilationArgs().entryPointName;
		const char* shaderTypeStr = ShaderTypeToString(shader->getCompilationArgs().shaderType);

		const uint64 pipelineLayoutNameXSH = shader->getPipelineLayoutNameXSH();
		const StringViewASCII pipelineLayoutName = StringViewASCII::FromCStr("XXX");
		// TODO: We should retrieve pipeline layout name from XSH reverse table.
		// Probably we might use table local to pipeline layouts just in case.

		const uint64 pipelineLayoutHash = shader->getPipelineLayout().getSourceHash();

		FmtPrint(fileWriter,
			"SH:", FmtArgHex64(shaderNameXSH, 16), '(', shaderName, ')', '|',
			"T:", shaderTypeStr, '|',
			"EP:", shaderEntryPointName, '|',
			"PL:", FmtArgHex64(pipelineLayoutNameXSH, 16), '(', pipelineLayoutName, ')', '|',
			"PLHash:", FmtArgHex64(pipelineLayoutHash, 16), '\n');
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

	// Load library definition file.
	LibraryDefinition libraryDefinition;
	FmtPrintStdOut("Loading shader library definition file '", cmdArgs.libraryDefinitionFilePath, "'\n");
	if (!LibraryDefinitionLoader::Load(libraryDefinition, cmdArgs.libraryDefinitionFilePath.getCStr()))
		return 1;

	// TODO:
#if 0
	FileSystem::CreateDirs(Path::RemoveFileName(cmdArgs.intermediateDirPath));
#endif

	// Sort pipelines by actual name, so log looks nice :sparkles:
	ArrayList<Shader*> shadersToCompile;
	{
		shadersToCompile.reserve(libraryDefinition.shaders.getSize());
		for (const ShaderRef& shader : libraryDefinition.shaders)
			shadersToCompile.pushBack(shader.get());

		QuickSort<Shader*>(shadersToCompile, shadersToCompile.getSize(),
			[](const Shader* left, const Shader* right) -> bool { return String::IsLess(left->getName(), right->getName()); });
	}

	InplaceStringASCIIx1024 librarySourceRootPath = Path::RemoveFileName(cmdArgs.libraryDefinitionFilePath);
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

	if (!StoreShaderLibrary(cmdArgs, libraryDefinition))
		return 1;

	StoreBuildCacheIndex(cmdArgs, libraryDefinition);

	FmtPrintStdOut("Shader library build succeeded\n");

	return 0;
}
