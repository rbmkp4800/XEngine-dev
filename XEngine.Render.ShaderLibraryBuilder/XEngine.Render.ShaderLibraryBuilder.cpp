#include <XLib.h>
#include <XLib.Algorithm.QuickSort.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.System.File.h>
#include <XLib.Text.h>

#include <XEngine.Render.ShaderLibraryFormat.h>

#include "XEngine.Render.ShaderLibraryBuilder.h"
#include "XEngine.Render.ShaderLibraryBuilder.LibraryDefinitionLoader.h"
#include "XEngine.Render.ShaderLibraryBuilder.SourceCache.h"

// TODO: Bytecode blobs deduplication (but probably we can just wait until we drop pipelines and move to separate shaders).

using namespace XLib;
using namespace XEngine::Render;
using namespace XEngine::Render::ShaderLibraryBuilder;

static void StoreShaderLibrary(const LibraryDefinition& libraryDefinition, const char* resultLibraryFilePath)
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
		const uint64 descriptorSetLayoutNameXSH = descriptorSetLayoutMapElement.key;
		const HAL::ShaderCompiler::DescriptorSetLayout* descriptorSetLayout = descriptorSetLayoutMapElement.value.get();

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
		const uint64 pipelineLayoutNameXSH = pipelineLayoutMapElement.key;
		const HAL::ShaderCompiler::PipelineLayout* pipelineLayout = pipelineLayoutMapElement.value.get();

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
		const uint64 pipelineNameXSH = pipelineMapElement.key;
		const Pipeline* pipeline = pipelineMapElement.value.get();
		const uint64 pipelineLayoutNameXSH = pipeline->pipelineLayoutNameXSH;
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

		if (pipeline->isGraphics)
		{
			const HAL::ShaderCompiler::Blob* stateBlob = pipeline->graphics.compiledBlobs.stateBlob.get();

			pipelineRecord.graphicsStateBlob.offset = nonBytecodeBlobsSizeAccum;
			pipelineRecord.graphicsStateBlob.size = stateBlob->getSize();
			pipelineRecord.graphicsStateBlob.checksum = stateBlob->getChecksum();

			nonBytecodeBlobsSizeAccum += stateBlob->getSize();
			nonBytecodeBlobs.emplaceBack(BlobDataView { stateBlob->getData(), stateBlob->getSize() });

			if (const HAL::ShaderCompiler::Blob* vsBlob = pipeline->graphics.compiledBlobs.vsBytecodeBlob.get())
			{
				pipelineRecord.vsBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { vsBlob->getData(), vsBlob->getSize(), vsBlob->getChecksum() });
			}
			if (const HAL::ShaderCompiler::Blob* asBlob = pipeline->graphics.compiledBlobs.asBytecodeBlob.get())
			{
				pipelineRecord.asBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { asBlob->getData(), asBlob->getSize(), asBlob->getChecksum() });
			}
			if (const HAL::ShaderCompiler::Blob* msBlob = pipeline->graphics.compiledBlobs.msBytecodeBlob.get())
			{
				pipelineRecord.msBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { msBlob->getData(), msBlob->getSize(), msBlob->getChecksum() });
			}
			if (const HAL::ShaderCompiler::Blob* psBlob = pipeline->graphics.compiledBlobs.psBytecodeBlob.get())
			{
				pipelineRecord.psORcsBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { psBlob->getData(), psBlob->getSize(), psBlob->getChecksum() });
			}
		}
		else
		{
			const HAL::ShaderCompiler::Blob* csBlob = pipeline->compute.compiledBlob.get();
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

int main()
{
	LibraryDefinition libraryDefinition;
	LibraryDefinitionLoader libraryDefinitionLoader(libraryDefinition);
	if (!libraryDefinitionLoader.load("../XEngine.Render.Shaders/.xeslibdef.xjson"))
		return 0;

	// Sort pipelines by actual name, so log looks nice :sparkles:

	ArrayList<Pipeline*> pipelinesToCompile;
	pipelinesToCompile.reserve(libraryDefinition.pipelines.getSize());
	for (const auto& pipelineMapElement : libraryDefinition.pipelines)
		pipelinesToCompile.pushBack(pipelineMapElement.value.get());

	QuickSort<Pipeline*>(pipelinesToCompile, pipelinesToCompile.getSize(),
		[](const Pipeline* left, const Pipeline* right) -> bool { return String::IsLess(left->getName(), right->getName()); });

	// Actual compilation.

	SourceCache sourceCache("../XEngine.Render.Shaders/");

	for (uint32 i = 0; i < pipelinesToCompile.getSize(); i++)
	{
		Pipeline& pipeline = *pipelinesToCompile[i];

		if (pipeline.isGraphics)
		{
			HAL::ShaderCompiler::GraphicsPipelineShaders shaders = pipeline.graphics.shaders; // Source text is zero. Resolve it.

			for (HAL::ShaderCompiler::ShaderDesc& shader : shaders.all)
			{
				if (shader.sourcePath.isEmpty())
					continue;

				if (!sourceCache.resolveText(shader.sourcePath, shader.sourceText))
				{
					return 1;
				}
			}

			HAL::ShaderCompiler::GraphicsPipelineCompilationArtifacts artifacts = {};

			const bool result = HAL::ShaderCompiler::CompileGraphicsPipeline(
				*pipeline.pipelineLayout, shaders, pipeline.graphics.settings, artifacts, pipeline.graphics.compiledBlobs);

			if (!result)
			{
				return 1;
			}

			// ...
		}
		else
		{
			HAL::ShaderCompiler::ShaderDesc shader = pipeline.compute.shader; // Source text is zero. Resolve it.

			if (!sourceCache.resolveText(shader.sourcePath, shader.sourceText))
			{
				return 1;
			}

			HAL::ShaderCompiler::ShaderCompilationArtifactsRef artifacts = nullptr;

			const bool result = HAL::ShaderCompiler::CompileComputePipeline(
				*pipeline.pipelineLayout, shader, artifacts, pipeline.compute.compiledBlob);

			if (!result)
			{
				return 1;
			}

			// ...
		}
	}

	// Store compiled shader library.

	StoreShaderLibrary(libraryDefinition, "../Build/XEngine.Render.Shaders.xeslib");

	return 0;
}
