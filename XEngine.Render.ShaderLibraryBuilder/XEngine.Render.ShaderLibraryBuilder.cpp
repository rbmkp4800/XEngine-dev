#include <XLib.h>
#include <XLib.Algorithm.QuickSort.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.System.File.h>
#include <XLib.Text.h>

#include <XEngine.Render.ShaderLibraryFormat.h>

#include "XEngine.Render.ShaderLibraryBuilder.h"
#include "XEngine.Render.ShaderLibraryBuilder.LibraryDefinitionLoader.h"
#include "XEngine.Render.ShaderLibraryBuilder.SourceCache.h"

// TODO: Library should contain objects in order of XSH increase.
// TODO: Bytecode blobs deduplication (but probably we can just wait until we drop pipelines and move to separate shaders).

using namespace XLib;
using namespace XEngine;
using namespace XEngine::Render;
using namespace XEngine::Render::ShaderLibraryBuilder;

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
		const GfxHAL::ShaderCompiler::DescriptorSetLayout* descriptorSetLayout = descriptorSetLayoutMapElement.ref.get();

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
		const GfxHAL::ShaderCompiler::PipelineLayout* pipelineLayout = pipelineLayoutMapElement.ref.get();

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
			const GfxHAL::ShaderCompiler::Blob* stateBlob = pipeline->graphicsCompiledBlobs().stateBlob.get();

			pipelineRecord.graphicsStateBlob.offset = nonBytecodeBlobsSizeAccum;
			pipelineRecord.graphicsStateBlob.size = stateBlob->getSize();
			pipelineRecord.graphicsStateBlob.checksum = stateBlob->getChecksum();

			nonBytecodeBlobsSizeAccum += stateBlob->getSize();
			nonBytecodeBlobs.emplaceBack(BlobDataView { stateBlob->getData(), stateBlob->getSize() });

			if (const GfxHAL::ShaderCompiler::Blob* vsBlob = pipeline->graphicsCompiledBlobs().vsBytecodeBlob.get())
			{
				pipelineRecord.vsBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { vsBlob->getData(), vsBlob->getSize(), vsBlob->getChecksum() });
			}
			if (const GfxHAL::ShaderCompiler::Blob* asBlob = pipeline->graphicsCompiledBlobs().asBytecodeBlob.get())
			{
				pipelineRecord.asBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { asBlob->getData(), asBlob->getSize(), asBlob->getChecksum() });
			}
			if (const GfxHAL::ShaderCompiler::Blob* msBlob = pipeline->graphicsCompiledBlobs().msBytecodeBlob.get())
			{
				pipelineRecord.msBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { msBlob->getData(), msBlob->getSize(), msBlob->getChecksum() });
			}
			if (const GfxHAL::ShaderCompiler::Blob* psBlob = pipeline->graphicsCompiledBlobs().psBytecodeBlob.get())
			{
				pipelineRecord.psORcsBytecodeBlobIndex = XCheckedCastU16(bytecodeBlobs.getSize());
				bytecodeBlobs.emplaceBack(BlobDataView { psBlob->getData(), psBlob->getSize(), psBlob->getChecksum() });
			}
		}
		else
		{
			const GfxHAL::ShaderCompiler::Blob* csBlob = pipeline->computeShaderCompiledBlob().get();
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
	GfxHAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
	const GfxHAL::ShaderCompiler::GraphicsPipelineShaders& shaders,
	const GfxHAL::ShaderCompiler::GraphicsPipelineSettings& settings)
{
	// I should not be allowed to code. Sorry :(

	uint32 memoryBlockSizeAccum = sizeof(Pipeline);

	const uint32 nameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += name.getLength() + 1;

	const uint32 vsSourcePathMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.vs.sourcePath.getLength() + 1;

	const uint32 vsEntryPointNameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.vs.entryPointName.getLength() + 1;

	const uint32 asSourcePathMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.as.sourcePath.getLength() + 1;

	const uint32 asEntryPointNameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.as.entryPointName.getLength() + 1;

	const uint32 msSourcePathMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.ms.sourcePath.getLength() + 1;

	const uint32 msEntryPointNameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.ms.entryPointName.getLength() + 1;

	const uint32 psSourcePathMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.ps.sourcePath.getLength() + 1;

	const uint32 psEntryPointNameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shaders.ps.entryPointName.getLength() + 1;

	const uint32 vertexBindingsMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += sizeof(GfxHAL::ShaderCompiler::VertexBindingDesc) * settings.vertexBindingCount;

	// NOTE: This code actually assumes that 'GfxHAL::ShaderCompiler::VertexBindingDesc::name' is inplace char array.
	// Because of this, we do not need to explicitly allocate memory for vertex binding names as they are stored inplace.
	XAssert(countof(settings.vertexBindings[0].name) == sizeof(settings.vertexBindings[0].name));

	const uint32 memoryBlockSize = memoryBlockSizeAccum;
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

	GfxHAL::ShaderCompiler::VertexBindingDesc* internalVertexBindings = (GfxHAL::ShaderCompiler::VertexBindingDesc*)(uintptr(memoryBlock) + vertexBindingsMemOffset);
	memoryCopy(internalVertexBindings, settings.vertexBindings, sizeof(GfxHAL::ShaderCompiler::VertexBindingDesc) * settings.vertexBindingCount);

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

	memoryCopy(&resultObject.graphics.settings.renderTargetsFormats, &settings.renderTargetsFormats, sizeof(resultObject.graphics.settings.renderTargetsFormats));
	resultObject.graphics.settings.depthStencilFormat = settings.depthStencilFormat;
	resultObject.isGraphics = true;

	return PipelineRef(&resultObject);
}

PipelineRef Pipeline::CreateCompute(StringViewASCII name,
	GfxHAL::ShaderCompiler::PipelineLayout* pipelineLayout, uint64 pipelineLayoutNameXSH,
	const GfxHAL::ShaderCompiler::ShaderDesc& shader)
{
	uint32 memoryBlockSizeAccum = sizeof(Pipeline);

	const uint32 nameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += name.getLength() + 1;

	const uint32 csSourcePathMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shader.sourcePath.getLength() + 1;

	const uint32 csEntryPointNameMemOffset = memoryBlockSizeAccum;
	memoryBlockSizeAccum += shader.entryPointName.getLength() + 1;

	const uint32 memoryBlockSize = memoryBlockSizeAccum;
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

int main()
{
	LibraryDefinition libraryDefinition;
	if (!LibraryDefinitionLoader::Load(libraryDefinition, "../XEngine.Render.Shaders/.xeslibdef.json"))
		return 0;

	// Sort pipelines by actual name, so log looks nice :sparkles:

	ArrayList<Pipeline*> pipelinesToCompile;
	pipelinesToCompile.reserve(libraryDefinition.pipelines.getSize());
	for (const auto& pipelineMapElement : libraryDefinition.pipelines)
		pipelinesToCompile.pushBack(pipelineMapElement.ref.get());

	QuickSort<Pipeline*>(pipelinesToCompile, pipelinesToCompile.getSize(),
		[](const Pipeline* left, const Pipeline* right) -> bool { return String::IsLess(left->getName(), right->getName()); });

	// Actual compilation.

	SourceCache sourceCache(StringView("../XEngine.Render.Shaders/"));

	for (uint32 i = 0; i < pipelinesToCompile.getSize(); i++)
	{
		Pipeline& pipeline = *pipelinesToCompile[i];

		InplaceStringASCIIx256 messageHeader;
		TextWriteFmt(messageHeader, " [", i + 1, "/", pipelinesToCompile.getSize(), "] ", pipeline.getName());
		TextWriteFmtStdOut(messageHeader, "\n");

		if (pipeline.isGraphicsPipeline())
		{
			GfxHAL::ShaderCompiler::GraphicsPipelineShaders shaders = pipeline.getGraphicsShaders();
			// Source text is zero. Resolve it.

			for (GfxHAL::ShaderCompiler::ShaderDesc& shader : shaders.all)
			{
				if (shader.sourcePath.isEmpty())
					continue;

				if (!sourceCache.resolveText(shader.sourcePath, shader.sourceText))
				{
					return 1;
				}
			}

			GfxHAL::ShaderCompiler::GraphicsPipelineCompilationArtifacts artifacts = {};
			GfxHAL::ShaderCompiler::GenericErrorMessage compilationErrorMessage = {};

			const bool result = GfxHAL::ShaderCompiler::CompileGraphicsPipeline(
				*pipeline.getPipelineLayout(), shaders, pipeline.getGraphicsSettings(),
				artifacts, pipeline.graphicsCompiledBlobs(), compilationErrorMessage);

			if (!result)
			{
				TextWriteFmtStdOut("graphics pipeline compilation error: ", compilationErrorMessage.text);
				return 1;
			}

			// ...
		}
		else
		{
			GfxHAL::ShaderCompiler::ShaderDesc shader = pipeline.getComputeShader();
			// Source text is zero. Resolve it.

			if (!sourceCache.resolveText(shader.sourcePath, shader.sourceText))
			{
				return 1;
			}

			GfxHAL::ShaderCompiler::ShaderCompilationArtifactsRef artifacts = nullptr;

			const bool result = GfxHAL::ShaderCompiler::CompileComputePipeline(
				*pipeline.getPipelineLayout(), shader, artifacts, pipeline.computeShaderCompiledBlob());

			if (!result)
			{
				return 1;
			}

			// ...
		}
	}

	// Store compiled shader library.
	TextWriteFmtStdOut("Storing shader library\n");
	StoreShaderLibrary(libraryDefinition, "../Build/XEngine.Render.Shaders.xeslib");

	return 0;
}
