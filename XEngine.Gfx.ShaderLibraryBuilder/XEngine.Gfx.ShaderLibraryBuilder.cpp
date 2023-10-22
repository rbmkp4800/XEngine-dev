#include <XLib.h>
#include <XLib.Algorithm.QuickSort.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Path.h>
#include <XLib.System.File.h>
#include <XLib.Text.h>

#include <XEngine.Gfx.ShaderLibraryFormat.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.LibraryDefinitionLoader.h"
#include "XEngine.Gfx.ShaderLibraryBuilder.SourceCache.h"

// TODO: Library should contain objects in order of XSH increase.
// TODO: Bytecode blobs deduplication (but probably we can just wait until we drop pipelines and move to separate shaders).

using namespace XLib;
using namespace XEngine::Gfx;
using namespace XEngine::Gfx::ShaderLibraryBuilder;

static void StoreShaderLibrary(LibraryDefinition& libraryDefinition, const char* resultLibraryFilePath)
{
	ArrayList<ShaderLibraryFormat::DescriptorSetLayoutRecord> descriptorSetLayoutRecords;
	ArrayList<ShaderLibraryFormat::PipelineLayoutRecord> pipelineLayoutRecords;
	ArrayList<ShaderLibraryFormat::PipelineRecord> pipelineRecords;
	ArrayList<ShaderLibraryFormat::BlobRecord> bytecodeBlobRecords;

	struct BlobDataView
	{
		const void* data;
		uint32 size;
		uint32 checksum; // Can be zero for non-bytecode blobs.
	};

	ArrayList<BlobDataView> nonBytecodeBlobs;
	ArrayList<BlobDataView> bytecodeBlobs;

	descriptorSetLayoutRecords.reserve(libraryDefinition.descriptorSetLayouts.getSize());
	pipelineLayoutRecords.reserve(libraryDefinition.pipelineLayouts.getSize());
	pipelineRecords.reserve(libraryDefinition.pipelines.getSize());

	nonBytecodeBlobs.reserve(
		libraryDefinition.descriptorSetLayouts.getSize() +
		libraryDefinition.pipelineLayouts.getSize() +
		libraryDefinition.pipelines.getSize());
	bytecodeBlobs.reserve(libraryDefinition.pipelines.getSize());

	uint32 nonBytecodeBlobsSizeAccum = 0;

	for (const auto& descriptorSetLayoutMapElement : libraryDefinition.descriptorSetLayouts) // Iterating in order of name hash increase.
	{
		const uint64 descriptorSetLayoutNameXSH = descriptorSetLayoutMapElement.nameXSH;
		const HAL::ShaderCompiler::DescriptorSetLayout* descriptorSetLayout = descriptorSetLayoutMapElement.ref.get();

		ShaderLibraryFormat::DescriptorSetLayoutRecord& descriptorSetLayoutRecord = descriptorSetLayoutRecords.emplaceBack();
		descriptorSetLayoutRecord = {};
		descriptorSetLayoutRecord.nameXSH0 = uint32(descriptorSetLayoutNameXSH);
		descriptorSetLayoutRecord.nameXSH1 = uint32(descriptorSetLayoutNameXSH >> 32);
		descriptorSetLayoutRecord.blob.offset = nonBytecodeBlobsSizeAccum;
		descriptorSetLayoutRecord.blob.size = descriptorSetLayout->getSerializedBlobSize();
		descriptorSetLayoutRecord.blob.checksum = descriptorSetLayout->getSerializedBlobChecksum();

		nonBytecodeBlobsSizeAccum += descriptorSetLayout->getSerializedBlobSize();

		nonBytecodeBlobs.emplaceBack(
			BlobDataView { descriptorSetLayout->getSerializedBlobData(), descriptorSetLayout->getSerializedBlobSize() });
	}

	for (const auto& pipelineLayoutMapElement : libraryDefinition.pipelineLayouts) // Iterating in order of name hash increase.
	{
		const uint64 pipelineLayoutNameXSH = pipelineLayoutMapElement.nameXSH;
		const HAL::ShaderCompiler::PipelineLayout* pipelineLayout = pipelineLayoutMapElement.ref.get();

		ShaderLibraryFormat::PipelineLayoutRecord& pipelineLayoutRecord = pipelineLayoutRecords.emplaceBack();
		pipelineLayoutRecord = {};
		pipelineLayoutRecord.nameXSH0 = uint32(pipelineLayoutNameXSH);
		pipelineLayoutRecord.nameXSH1 = uint32(pipelineLayoutNameXSH >> 32);
		pipelineLayoutRecord.blob.offset = nonBytecodeBlobsSizeAccum;
		pipelineLayoutRecord.blob.size = pipelineLayout->getSerializedBlobSize();
		pipelineLayoutRecord.blob.checksum = pipelineLayout->getSerializedBlobChecksum();

		nonBytecodeBlobsSizeAccum += pipelineLayout->getSerializedBlobSize();

		nonBytecodeBlobs.emplaceBack(
			BlobDataView { pipelineLayout->getSerializedBlobData(), pipelineLayout->getSerializedBlobSize() });
	}

	for (const auto& pipelineMapElement : libraryDefinition.pipelines) // Iterating in order of name hash increase.
	{
		const uint64 pipelineNameXSH = pipelineMapElement.nameXSH;
		const Pipeline* pipeline = pipelineMapElement.ref.get();
		const uint64 pipelineLayoutNameXSH = pipeline->getPipelineLayoutNameXSH();
		XAssert(pipelineLayoutNameXSH);

		ShaderLibraryFormat::PipelineRecord& pipelineRecord = pipelineRecords.emplaceBack();
		pipelineRecord = {};
		pipelineRecord.nameXSH0 = uint32(pipelineNameXSH);
		pipelineRecord.nameXSH1 = uint32(pipelineNameXSH >> 32);
		pipelineRecord.pipelineLayoutNameXSH0 = uint32(pipelineLayoutNameXSH);
		pipelineRecord.pipelineLayoutNameXSH1 = uint32(pipelineLayoutNameXSH >> 32);
		
		pipelineRecord.vsBytecodeBlobIndex = -1;
		pipelineRecord.asBytecodeBlobIndex = -1;
		pipelineRecord.msBytecodeBlobIndex = -1;
		pipelineRecord.psORcsBytecodeBlobIndex = -1;

		if (pipeline->isGraphicsPipeline())
		{
			const HAL::ShaderCompiler::Blob* stateBlob = pipeline->graphicsCompiledBlobs().stateBlob.get();

			pipelineRecord.graphicsStateBlob.offset = nonBytecodeBlobsSizeAccum;
			pipelineRecord.graphicsStateBlob.size = stateBlob->getSize();
			pipelineRecord.graphicsStateBlob.checksum = stateBlob->getChecksum();

			nonBytecodeBlobsSizeAccum += stateBlob->getSize();
			nonBytecodeBlobs.emplaceBack(BlobDataView { stateBlob->getData(), stateBlob->getSize() });

			if (const HAL::ShaderCompiler::Blob* vsBlob = pipeline->graphicsCompiledBlobs().vsBytecodeBlob.get())
			{
				pipelineRecord.vsBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { vsBlob->getData(), vsBlob->getSize(), vsBlob->getChecksum() });
			}
			if (const HAL::ShaderCompiler::Blob* asBlob = pipeline->graphicsCompiledBlobs().asBytecodeBlob.get())
			{
				pipelineRecord.asBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { asBlob->getData(), asBlob->getSize(), asBlob->getChecksum() });
			}
			if (const HAL::ShaderCompiler::Blob* msBlob = pipeline->graphicsCompiledBlobs().msBytecodeBlob.get())
			{
				pipelineRecord.msBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { msBlob->getData(), msBlob->getSize(), msBlob->getChecksum() });
			}
			if (const HAL::ShaderCompiler::Blob* psBlob = pipeline->graphicsCompiledBlobs().psBytecodeBlob.get())
			{
				pipelineRecord.psORcsBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { psBlob->getData(), psBlob->getSize(), psBlob->getChecksum() });
			}
		}
		else
		{
			const HAL::ShaderCompiler::Blob* csBlob = pipeline->computeShaderCompiledBlob().get();
			pipelineRecord.psORcsBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
			bytecodeBlobs.emplaceBack(BlobDataView { csBlob->getData(), csBlob->getSize(), csBlob->getChecksum() });
		}
	}

	const uint32 nonBytecodeBlobsTotalSize = nonBytecodeBlobsSizeAccum;

	bytecodeBlobRecords.reserve(bytecodeBlobs.getSize());
	uint32 bytecodeBlobsSizeAccum = 0;

	for (const BlobDataView& blob : bytecodeBlobs)
	{
		ShaderLibraryFormat::BlobRecord& record = bytecodeBlobRecords.emplaceBack();
		record = {};
		record.offset = nonBytecodeBlobsTotalSize + bytecodeBlobsSizeAccum;
		record.size = blob.size;
		record.checksum = blob.checksum;
		bytecodeBlobsSizeAccum += blob.size;
	}

	const uint32 bytecodeBlobsTotalSize = bytecodeBlobsSizeAccum;

	const uintptr blobsDataOffset =
		sizeof(ShaderLibraryFormat::LibraryHeader) +
		descriptorSetLayoutRecords.getByteSize() +
		pipelineLayoutRecords.getByteSize() +
		pipelineRecords.getByteSize() +
		bytecodeBlobRecords.getByteSize();

	File file;
	file.open(resultLibraryFilePath, FileAccessMode::Write, FileOpenMode::Override);

	ShaderLibraryFormat::LibraryHeader header = {};
	header.signature = ShaderLibraryFormat::LibrarySignature;
	header.version = ShaderLibraryFormat::LibraryCurrentVersion;
	header.descriptorSetLayoutCount = XCheckedCastU16(descriptorSetLayoutRecords.getSize());
	header.pipelineLayoutCount = XCheckedCastU16(pipelineLayoutRecords.getSize());
	header.pipelineCount = XCheckedCastU16(pipelineRecords.getSize());
	header.bytecodeBlobCount = XCheckedCastU16(bytecodeBlobRecords.getSize());
	header.blobsDataOffset = XCheckedCastU32(blobsDataOffset);
	file.write(header);

	file.write(descriptorSetLayoutRecords.getData(), descriptorSetLayoutRecords.getByteSize());
	file.write(pipelineLayoutRecords.getData(), pipelineLayoutRecords.getByteSize());
	file.write(pipelineRecords.getData(), pipelineRecords.getByteSize());
	file.write(bytecodeBlobRecords.getData(), bytecodeBlobRecords.getByteSize());

	uint32 nonBytecodeBlobsTotalSizeCheck = 0;
	for (const BlobDataView& blob : nonBytecodeBlobs)
	{
		file.write(blob.data, blob.size);
		nonBytecodeBlobsTotalSizeCheck += blob.size;
	}

	uint32 bytecodeBlobsTotalSizeCheck = 0;
	for (const BlobDataView& blob : bytecodeBlobs)
	{
		file.write(blob.data, blob.size);
		bytecodeBlobsTotalSizeCheck += blob.size;
	}

	XAssert(nonBytecodeBlobsTotalSizeCheck == nonBytecodeBlobsTotalSize);
	XAssert(bytecodeBlobsTotalSizeCheck == bytecodeBlobsTotalSize);

	file.close();
}

PipelineRef Pipeline::CreateGraphics(StringViewASCII name,
	HAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
	const HAL::ShaderCompiler::GraphicsPipelineShaders& shaders,
	const HAL::ShaderCompiler::GraphicsPipelineSettings& settings)
{
	// I should not be allowed to code. Sorry :(

	uintptr memoryBlockSizeAccum = sizeof(Pipeline);

	const uintptr nameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += name.getLength() + 1;

	const uintptr vsSourcePathMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.vs.sourcePath.getLength() + 1;

	const uintptr vsEntryPointNameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.vs.entryPointName.getLength() + 1;

	const uintptr asSourcePathMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.as.sourcePath.getLength() + 1;

	const uintptr asEntryPointNameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.as.entryPointName.getLength() + 1;

	const uintptr msSourcePathMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.ms.sourcePath.getLength() + 1;

	const uintptr msEntryPointNameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.ms.entryPointName.getLength() + 1;

	const uintptr psSourcePathMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.ps.sourcePath.getLength() + 1;

	const uintptr psEntryPointNameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.ps.entryPointName.getLength() + 1;

	const uintptr vertexBindingsMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(HAL::ShaderCompiler::VertexBindingDesc) * settings.vertexBindingCount;

	// NOTE: This code actually assumes that 'HAL::ShaderCompiler::VertexBindingDesc::name' is inplace char array.
	// Because of this, we do not need to explicitly allocate memory for vertex binding names as they are stored inplace.
	XAssert(countof(settings.vertexBindings[0].nameCStr) == sizeof(settings.vertexBindings[0].nameCStr));

	const uintptr memoryBlockSize = memoryBlockSizeAccum;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize);

	memoryCopy((char*)memoryBlock + nameMemOffset, name.getData(), name.getLength());
	memoryCopy((char*)memoryBlock + vsSourcePathMemOffset, shaders.vs.sourcePath.getData(), shaders.vs.sourcePath.getLength());
	memoryCopy((char*)memoryBlock + vsEntryPointNameMemOffset, shaders.vs.entryPointName.getData(), shaders.vs.entryPointName.getLength());
	memoryCopy((char*)memoryBlock + asSourcePathMemOffset, shaders.as.sourcePath.getData(), shaders.as.sourcePath.getLength());
	memoryCopy((char*)memoryBlock + asEntryPointNameMemOffset, shaders.as.entryPointName.getData(), shaders.as.entryPointName.getLength());
	memoryCopy((char*)memoryBlock + msSourcePathMemOffset, shaders.ms.sourcePath.getData(), shaders.ms.sourcePath.getLength());
	memoryCopy((char*)memoryBlock + msEntryPointNameMemOffset, shaders.ms.entryPointName.getData(), shaders.ms.entryPointName.getLength());
	memoryCopy((char*)memoryBlock + psSourcePathMemOffset, shaders.ps.sourcePath.getData(), shaders.ps.sourcePath.getLength());
	memoryCopy((char*)memoryBlock + psEntryPointNameMemOffset, shaders.ps.entryPointName.getData(), shaders.ps.entryPointName.getLength());

	HAL::ShaderCompiler::VertexBindingDesc* internalVertexBindings = (HAL::ShaderCompiler::VertexBindingDesc*)(uintptr(memoryBlock) + vertexBindingsMemOffset);
	memoryCopy(internalVertexBindings, settings.vertexBindings, sizeof(HAL::ShaderCompiler::VertexBindingDesc) * settings.vertexBindingCount);

	Pipeline& resultObject = *(Pipeline*)memoryBlock;
	construct(resultObject);

	resultObject.name = StringViewASCII((char*)memoryBlock + nameMemOffset, name.getLength());
	resultObject.pipelineLayout = pipelineLayout;
	resultObject.pipelineLayoutNameXSH = pipelineLayoutNameXSH;
	resultObject.graphics.shaders.vs.sourcePath = StringView((char*)memoryBlock + vsSourcePathMemOffset, shaders.vs.sourcePath.getLength());
	resultObject.graphics.shaders.vs.entryPointName = StringView((char*)memoryBlock + vsEntryPointNameMemOffset, shaders.vs.entryPointName.getLength());
	resultObject.graphics.shaders.as.sourcePath = StringView((char*)memoryBlock + asSourcePathMemOffset, shaders.as.sourcePath.getLength());
	resultObject.graphics.shaders.as.entryPointName = StringView((char*)memoryBlock + asEntryPointNameMemOffset, shaders.as.entryPointName.getLength());
	resultObject.graphics.shaders.ms.sourcePath = StringView((char*)memoryBlock + msSourcePathMemOffset, shaders.ms.sourcePath.getLength());
	resultObject.graphics.shaders.ms.entryPointName = StringView((char*)memoryBlock + msEntryPointNameMemOffset, shaders.ms.entryPointName.getLength());
	resultObject.graphics.shaders.ps.sourcePath = StringView((char*)memoryBlock + psSourcePathMemOffset, shaders.ps.sourcePath.getLength());
	resultObject.graphics.shaders.ps.entryPointName = StringView((char*)memoryBlock + psEntryPointNameMemOffset, shaders.ps.entryPointName.getLength());

	memoryCopy(&resultObject.graphics.settings.vertexBuffers, &settings.vertexBuffers, sizeof(resultObject.graphics.settings.vertexBuffers));
	resultObject.graphics.settings.vertexBindings = settings.vertexBindingCount > 0 ? internalVertexBindings : nullptr;
	resultObject.graphics.settings.vertexBindingCount = settings.vertexBindingCount;

	memoryCopy(&resultObject.graphics.settings.colorRTFormats, &settings.colorRTFormats, sizeof(resultObject.graphics.settings.colorRTFormats));
	resultObject.graphics.settings.depthStencilRTFormat = settings.depthStencilRTFormat;
	resultObject.isGraphics = true;

	return PipelineRef(&resultObject);
}

PipelineRef Pipeline::CreateCompute(StringViewASCII name,
	HAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
	const HAL::ShaderCompiler::ShaderDesc& shader)
{
	uintptr memoryBlockSizeAccum = sizeof(Pipeline);

	const uintptr nameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += name.getLength() + 1;

	const uintptr csSourcePathMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shader.sourcePath.getLength() + 1;

	const uintptr csEntryPointNameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shader.entryPointName.getLength() + 1;

	const uintptr memoryBlockSize = memoryBlockSizeAccum;
	void* memoryBlock = SystemHeapAllocator::Allocate(memoryBlockSize);
	memorySet(memoryBlock, 0, memoryBlockSize);

	memoryCopy((char*)memoryBlock + nameMemOffset, name.getData(), name.getLength());
	memoryCopy((char*)memoryBlock + csSourcePathMemOffset, shader.sourcePath.getData(), shader.sourcePath.getLength());
	memoryCopy((char*)memoryBlock + csEntryPointNameMemOffset, shader.entryPointName.getData(), shader.entryPointName.getLength());

	Pipeline& resultObject = *(Pipeline*)memoryBlock;
	construct(resultObject);

	resultObject.name = StringViewASCII((char*)memoryBlock + nameMemOffset, name.getLength());
	resultObject.pipelineLayout = pipelineLayout;
	resultObject.pipelineLayoutNameXSH = pipelineLayoutNameXSH;
	resultObject.compute.shader.sourcePath = StringView((char*)memoryBlock + csSourcePathMemOffset, shader.sourcePath.getLength());
	resultObject.compute.shader.entryPointName = StringView((char*)memoryBlock + csEntryPointNameMemOffset, shader.entryPointName.getLength());
	resultObject.isGraphics = false;

	return PipelineRef(&resultObject);
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

	ArrayList<Pipeline*> pipelinesToCompile;
	pipelinesToCompile.reserve(libraryDefinition.pipelines.getSize());
	for (const auto& pipelineMapElement : libraryDefinition.pipelines)
		pipelinesToCompile.pushBack(pipelineMapElement.ref.get());

	QuickSort<Pipeline*>(pipelinesToCompile, pipelinesToCompile.getSize(),
		[](const Pipeline* left, const Pipeline* right) -> bool { return String::IsLess(left->getName(), right->getName()); });

	// Actual compilation.

	SourceCache sourceCache(Path::RemoveFileName(libraryDefinitionFilePath));

	for (uint32 i = 0; i < pipelinesToCompile.getSize(); i++)
	{
		Pipeline& pipeline = *pipelinesToCompile[i];

		InplaceStringASCIIx256 messageHeader;
		TextWriteFmt(messageHeader, " [", i + 1, "/", pipelinesToCompile.getSize(), "] ", pipeline.getName());
		TextWriteFmtStdOut(messageHeader, "\n");

		if (pipeline.isGraphicsPipeline())
		{
			HAL::ShaderCompiler::GraphicsPipelineShaders shaders = pipeline.getGraphicsShaders();
			// Source text is zero. Resolve it.

			for (HAL::ShaderCompiler::ShaderDesc& shader : shaders.all)
			{
				if (shader.sourcePath.isEmpty())
					continue;

				if (!sourceCache.resolveText(shader.sourcePath, shader.sourceText))
				{
					return 0;
				}
			}

			HAL::ShaderCompiler::GraphicsPipelineCompilationArtifacts artifacts = {};
			HAL::ShaderCompiler::GenericErrorMessage compilationErrorMessage = {};

			const bool result = HAL::ShaderCompiler::CompileGraphicsPipeline(
				*pipeline.getPipelineLayout(), shaders, pipeline.getGraphicsSettings(),
				artifacts, pipeline.graphicsCompiledBlobs(), compilationErrorMessage);

			if (!result)
			{
				TextWriteFmtStdOut("graphics pipeline compilation error: ", compilationErrorMessage.text, "\n");
				return 0;
			}

			// ...
		}
		else
		{
			HAL::ShaderCompiler::ShaderDesc shader = pipeline.getComputeShader();
			// Source text is zero. Resolve it.

			if (!sourceCache.resolveText(shader.sourcePath, shader.sourceText))
			{
				return 0;
			}

			HAL::ShaderCompiler::ShaderCompilationArtifactsRef artifacts = nullptr;

			const bool result = HAL::ShaderCompiler::CompileComputePipeline(
				*pipeline.getPipelineLayout(), shader, artifacts, pipeline.computeShaderCompiledBlob());

			if (!result)
			{
				return 0;
			}

			// ...
		}
	}

	// Store compiled shader library.

	TextWriteFmtStdOut("Storing shader library '", outLibraryFilePath, "'\n");

	StoreShaderLibrary(libraryDefinition, outLibraryFilePath);

	return 0;
}
