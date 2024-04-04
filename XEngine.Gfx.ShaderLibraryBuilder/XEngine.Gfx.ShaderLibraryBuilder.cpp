#include <XLib.h>
#include <XLib.Algorithm.QuickSort.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.CRC.h>
#include <XLib.Path.h>
#include <XLib.System.File.h>
#include <XLib.Text.h>

#include <XEngine.Gfx.ShaderLibraryFormat.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.LibraryDefinitionLoader.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.SourceCache.h"

using namespace XLib;
using namespace XEngine::Gfx;
using namespace XEngine::Gfx::ShaderLibraryBuilder;

ShaderRef Shader::Create(XLib::StringViewASCII name,
	HAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
	const HAL::ShaderCompiler::ShaderCompilationArgs& compilationArgs)
{
	// I should not be allowed to code. Sorry :(

	uintptr memoryBlockSizeAccum = sizeof(Shader);

	const uintptr nameStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += name.getLength() + 1;

	const uintptr sourcePathStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += compilationArgs.sourcePath.getLength() + 1;

	const uintptr entryPointNameStrOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += compilationArgs.entryPointName.getLength() + 1;

#if 0
	const uintptr vertexBindingsMemOffset = memoryBlockSizeAccum;
	if (compilationArgs.type == HAL::ShaderType::Vertex)
		memoryBlockSizeAccum += sizeof(HAL::ShaderCompiler::VertexInputBindingDesc) * compilationArgs.vs.vertexInputBindingCount;

	// NOTE: This code actually assumes that 'HAL::ShaderCompiler::VertexBindingDesc::name' is inplace char array.
	// Because of this, we do not need to explicitly allocate memory for vertex binding names as they are stored inplace.
	XAssert(countof(compilationArgs.vs.vertexInputBindings[0].nameCStr) == sizeof(compilationArgs.vs.vertexInputBindings[0].nameCStr));
#endif

	const uintptr memoryBlockSize = memoryBlockSizeAccum;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize);

	memoryCopy((char*)memoryBlock + nameStrOffset, name.getData(), name.getLength());
	memoryCopy((char*)memoryBlock + sourcePathStrOffset, compilationArgs.sourcePath.getData(), compilationArgs.sourcePath.getLength());
	memoryCopy((char*)memoryBlock + entryPointNameStrOffset, compilationArgs.entryPointName.getData(), compilationArgs.entryPointName.getLength());

	Shader& resultObject = *(Shader*)memoryBlock;
	construct(resultObject);

	resultObject.name = StringViewASCII((char*)memoryBlock + nameStrOffset, name.getLength());
	resultObject.pipelineLayout = pipelineLayout;
	resultObject.pipelineLayoutNameXSH = pipelineLayoutNameXSH;
	resultObject.compilationArgs.sourcePath = StringViewASCII((char*)memoryBlock + sourcePathStrOffset, compilationArgs.sourcePath.getLength());
	resultObject.compilationArgs.entryPointName = StringViewASCII((char*)memoryBlock + entryPointNameStrOffset, compilationArgs.entryPointName.getLength());
	resultObject.compilationArgs.shaderType = compilationArgs.shaderType;

#if 0
	if (compilationArgs.type == HAL::ShaderType::Vertex)
	{
		HAL::ShaderCompiler::VertexInputBindingDesc* internalVertexInputBindings = (HAL::ShaderCompiler::VertexInputBindingDesc*)(uintptr(memoryBlock) + vertexBindingsMemOffset);
		memoryCopy(internalVertexInputBindings, compilationArgs.vs.vertexInputBindings, sizeof(HAL::ShaderCompiler::VertexInputBindingDesc) * compilationArgs.vs.vertexInputBindingCount);

		memoryCopy(&resultObject.compilationArgs.vs.vertexBuffers, &compilationArgs.vs.vertexBuffers, sizeof(compilationArgs.vs.vertexBuffers));
		resultObject.compilationArgs.vs.vertexInputBindings = compilationArgs.vs.vertexInputBindingCount > 0 ? internalVertexInputBindings : nullptr;
		resultObject.compilationArgs.vs.vertexInputBindingCount = compilationArgs.vs.vertexInputBindingCount;
	}
	else if (compilationArgs.type == HAL::ShaderType::Pixel)
	{
		memoryCopy(&resultObject.compilationArgs.ps.colorRTFormats, &compilationArgs.ps.colorRTFormats, sizeof(compilationArgs.ps.colorRTFormats));
		resultObject.compilationArgs.ps.depthStencilRTFormat = compilationArgs.ps.depthStencilRTFormat;
	}
#endif

	return ShaderRef(&resultObject);
}

static void StoreShaderLibrary(const LibraryDefinition& libraryDefinition, const char* resultLibraryFilePath)
{
	XTODO("Sort objects in order of XSH increase");

	ArrayList<ShaderLibraryFormat::DescriptorSetLayoutRecord> descriptorSetLayoutRecords;
	ArrayList<ShaderLibraryFormat::PipelineLayoutRecord> pipelineLayoutRecords;
	ArrayList<ShaderLibraryFormat::ShaderRecord> shaderRecords;

	descriptorSetLayoutRecords.reserve(libraryDefinition.descriptorSetLayouts.getSize());
	pipelineLayoutRecords.reserve(libraryDefinition.pipelineLayouts.getSize());
	shaderRecords.reserve(libraryDefinition.shaders.getSize());

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

	const uint32 blobsDataTotalSize = blobsDataSizeAccum;

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
	header.blobsDataOffset = XCheckedCastU32(blobsDataOffset);

	File file;
	file.open(resultLibraryFilePath, FileAccessMode::Write, FileOpenMode::Override);

	file.write(header);
	file.write(descriptorSetLayoutRecords.getData(), descriptorSetLayoutRecords.getByteSize());
	file.write(pipelineLayoutRecords.getData(), pipelineLayoutRecords.getByteSize());
	file.write(shaderRecords.getData(), shaderRecords.getByteSize());

	uint32 blobsDataTotalSizeCheck = 0;
	for (const BlobDataView& blob : blobs)
	{
		file.write(blob.data, blob.size);
		blobsDataTotalSizeCheck += blob.size;
	}
	XAssert(blobsDataTotalSize == blobsDataTotalSizeCheck);

	file.close();
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

	if (!libraryDefinitionFilePath)
	{
		TextWriteFmtStdOut("error: missing library definition file path");
		return 0;
	}
	if (!outLibraryFilePath)
	{
		TextWriteFmtStdOut("error: missing output file path");
		return 0;
	}

	// Load library definition file.

	TextWriteFmtStdOut("Loading shader library definition file '", libraryDefinitionFilePath, "'\n");

	LibraryDefinition libraryDefinition;
	if (!LibraryDefinitionLoader::Load(libraryDefinition, libraryDefinitionFilePath))
		return 0;

	// Sort pipelines by actual name, so log looks nice :sparkles:

	ArrayList<Shader*> shadersToCompile;
	shadersToCompile.reserve(libraryDefinition.shaders.getSize());
	for (const ShaderRef& shader : libraryDefinition.shaders)
		shadersToCompile.pushBack(shader.get());

	QuickSort<Shader*>(shadersToCompile, shadersToCompile.getSize(),
		[](const Shader* left, const Shader* right) -> bool { return String::IsLess(left->getName(), right->getName()); });

	// Actual compilation.
	SourceCache sourceCache(Path::RemoveFileName(libraryDefinitionFilePath));
	for (uint32 i = 0; i < shadersToCompile.getSize(); i++)
	{
		Shader& shader = *shadersToCompile[i];

		InplaceStringASCIIx256 messageHeader;
		TextWriteFmt(messageHeader, " [", i + 1, "/", shadersToCompile.getSize(), "] ", shader.getName());
		TextWriteFmtStdOut(messageHeader, "\n");

		StringViewASCII sourceText;
		if (!sourceCache.resolveText(shader.getCompilationArgs().sourcePath, sourceText))
		{
			TextWriteFmtStdOut("Failed to resolve source text\n");
			return 0;
		}

		HAL::ShaderCompiler::ShaderCompilationResultRef compilationResult =
			HAL::ShaderCompiler::CompileShader(sourceText, shader.getPipelineLayout(), shader.getCompilationArgs());

		if (compilationResult->getStatus() != HAL::ShaderCompiler::ShaderCompilationStatus::Success)
		{
			TextWriteFmtStdOut("Shader compilation error: ", compilationResult->getPlatformCompilerOutput(), "\n");
			return 0;
		}

		shader.setCompiledBlob(compilationResult->getBytecodeBlob());
	}

	// Store compiled shader library.
	TextWriteFmtStdOut("Storing shader library '", outLibraryFilePath, "'\n");
	StoreShaderLibrary(libraryDefinition, outLibraryFilePath);

	return 0;
}
