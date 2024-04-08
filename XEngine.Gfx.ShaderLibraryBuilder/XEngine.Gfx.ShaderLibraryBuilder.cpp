#include <XLib.h>
#include <XLib.Algorithm.QuickSort.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.CRC.h>
#include <XLib.Path.h>
#include <XLib.System.File.h>
#include <XLib.Text.h>

#include <XEngine.Gfx.ShaderLibraryFormat.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.LibraryDefinition.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.LibraryDefinitionLoader.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.SourceCache.h"

using namespace XLib;
using namespace XEngine::Gfx;
using namespace XEngine::Gfx::ShaderLibraryBuilder;

static void StoreShaderLibrary(const LibraryDefinition& libraryDefinition, const char* resultLibraryFilePath)
{
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

	File file;
	file.open(resultLibraryFilePath, FileAccessMode::Write, FileOpenMode::Override);

	file.write(header);
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
}

static HAL::ShaderCompiler::SourceResolutionResult ResolveSource(void* context, StringViewASCII sourceFilename)
{
	SourceCache& sourceCache = *(SourceCache*)context;
	HAL::ShaderCompiler::SourceResolutionResult result = {};
	result.resolved = sourceCache.resolve(sourceFilename, result.text);
	return result;
}

int main(int argc, char* argv[])
{
	// Parse cmd line arguments.

	const char* libraryDefinitionFilePath = nullptr;
	const char* outLibraryFilePath = nullptr;

	for (int i = 1; i < argc; i++)
	{
		if (String::StartsWith(argv[i], "-libdef="))
			libraryDefinitionFilePath = argv[i] + 8;
		else if (String::StartsWith(argv[i], "-out="))
			outLibraryFilePath = argv[i] + 5;
	}

	// TODO: Check -xxx="A B"

	if (!libraryDefinitionFilePath)
	{
		TextWriteFmtStdOut("error: missing library definition file path (-libdef=XXX)");
		return 0;
	}
	if (!outLibraryFilePath)
	{
		TextWriteFmtStdOut("error: missing output file path (-out=XXX)");
		return 0;
	}

	// Load library definition file.
	LibraryDefinition libraryDefinition;
	TextWriteFmtStdOut("Loading shader library definition file '", libraryDefinitionFilePath, "'\n");
	if (!LibraryDefinitionLoader::Load(libraryDefinition, libraryDefinitionFilePath))
		return 0;

	// Get library source root path.
	InplaceStringASCIIx1024 librarySourceRootPath;
	{
		InplaceStringASCIIx1024 a;
		a.copyFrom(Path::RemoveFileName(libraryDefinitionFilePath));
		if (!a.isEmpty())
			a.append('/');
		a.append('.');
		Path::Normalize(a, librarySourceRootPath);
		if (!Path::HasTrailingDirectorySeparator(librarySourceRootPath))
			librarySourceRootPath.append('/');
	}

	// Sort pipelines by actual name, so log looks nice :sparkles:
	ArrayList<Shader*> shadersToCompile;
	{
		shadersToCompile.reserve(libraryDefinition.shaders.getSize());
		for (const ShaderRef& shader : libraryDefinition.shaders)
			shadersToCompile.pushBack(shader.get());

		QuickSort<Shader*>(shadersToCompile, shadersToCompile.getSize(),
			[](const Shader* left, const Shader* right) -> bool { return String::IsLess(left->getName(), right->getName()); });
	}

	SourceCache sourceCache;

	// Actual compilation.
	for (uint32 i = 0; i < shadersToCompile.getSize(); i++)
	{
		Shader& shader = *shadersToCompile[i];

		InplaceStringASCIIx256 messageHeader;
		TextWriteFmt(messageHeader, " [", i + 1, "/", shadersToCompile.getSize(), "] ", shader.getName());
		TextWriteFmtStdOut(messageHeader, "\n");

		InplaceStringASCIIx1024 mainSourceFilePath;
		mainSourceFilePath.copyFrom(librarySourceRootPath);
		mainSourceFilePath.append(shader.getMainSourceFilename());

		HAL::ShaderCompiler::ShaderCompilationResultRef compilationResult =
			HAL::ShaderCompiler::CompileShader(
				mainSourceFilePath, shader.getCompilationArgs(), shader.getPipelineLayout(),
				&ResolveSource, &sourceCache);

		if (compilationResult->getPreprocessingOuput().getLength() > 0)
			TextWriteFmtStdOut(compilationResult->getPreprocessingOuput(), "\n");
		if (compilationResult->getCompilationOutput().getLength() > 0)
			TextWriteFmtStdOut(compilationResult->getCompilationOutput(), "\n");

		if (compilationResult->getStatus() != HAL::ShaderCompiler::ShaderCompilationStatus::Success)
			return 0;

		shader.setCompiledBlob(compilationResult->getBytecodeBlob());
	}

	// Store compiled shader library.
	TextWriteFmtStdOut("Storing shader library '", outLibraryFilePath, "'\n");
	StoreShaderLibrary(libraryDefinition, outLibraryFilePath);

	return 0;
}
