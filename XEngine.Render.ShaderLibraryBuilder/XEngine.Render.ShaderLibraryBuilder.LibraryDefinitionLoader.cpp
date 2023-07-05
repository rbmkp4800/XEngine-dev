#include <XLib.String.h>
#include <XLib.System.File.h>

#include <XEngine.XStringHash.h>

#include "XEngine.Render.ShaderLibraryBuilder.LibraryDefinitionLoader.h"

using namespace XLib;
using namespace XEngine::Render;
using namespace XEngine::Render::ShaderLibraryBuilder;

XTODO("Any object name (DSL / pipeline layout / pipeline) should be valid filename");

static bool ParseTexelViewFormatString(StringViewASCII string, HAL::TexelViewFormat& resultFormat)
{
	if (string == "R8G8B8BA8_UNORM" || string == "R8G8B8BA8_Unorm")
	{
		resultFormat = HAL::TexelViewFormat::R8G8B8A8_UNORM;
		return true;
	}
	return false;
}

static bool ParseDepthStencilFormatString(StringViewASCII string, HAL::DepthStencilFormat& resultFormat)
{
	if (string == "D32")
	{
		resultFormat = HAL::DepthStencilFormat::D32;
		return true;
	}
	return false;
}

void LibraryDefinitionLoader::reportError(const char* message, const XJSON::Location& location)
{
	TextWriteFmtStdOut(xjsonPath, ':',
		location.lineNumber, ':', location.columnNumber, ": error: ", message, '\n');
}

void LibraryDefinitionLoader::reportXJSONError(const XJSON::ParserError& xjsonError)
{
	TextWriteFmtStdOut(xjsonPath, ':',
		xjsonError.location.lineNumber, ':', xjsonError.location.columnNumber,
		": error: XJSON: ", xjsonError.message, '\n');
}

bool LibraryDefinitionLoader::readDescriptorSetLayoutDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty)
{
	if (xjsonEntryDeclarationProperty.value.valueType != XJSON::ValueType::Object)
	{
		reportError("descriptor set layout declaration property value should be XJSON object",
			xjsonEntryDeclarationProperty.value.valueLocation);
		return false;
	}

	InplaceArrayList<HAL::ShaderCompiler::DescriptorSetBindingDesc, HAL::MaxDescriptorSetBindingCount> bindings;

	xjsonParser.openObjectValueScope();
	for (;;)
	{
		XJSON::KeyValue xjsonProperty = {};
		XJSON::ParserError xjsonError = {};
		const XJSON::ReadStatus xjsonReadStatus = xjsonParser.readKeyValue(xjsonProperty, xjsonError);

		if (xjsonReadStatus == XJSON::ReadStatus::EndOfScope)
			break;
		if (xjsonReadStatus == XJSON::ReadStatus::Error)
		{
			reportXJSONError(xjsonError);
			return false;
		}
		XAssert(xjsonReadStatus == XJSON::ReadStatus::Success);

		if (xjsonProperty.key.isEmpty())
		{
			reportError("descriptor binding name can not be empty", xjsonProperty.keyLocation);
			return false;
		}
		if (xjsonProperty.value.typeAnnotation.isEmpty())
		{
			reportError("descriptor type not specified", xjsonProperty.keyLocation);
			return false;
		}
		if (xjsonProperty.value.valueType != XJSON::ValueType::None)
		{
			reportError("value should be empty", xjsonProperty.value.valueLocation);
			return false;
		}

		HAL::ShaderCompiler::DescriptorSetBindingDesc binding = {};
		binding.name = xjsonProperty.key;
		binding.descriptorCount = 1;

		if (xjsonProperty.value.typeAnnotation == "read-only-buffer-descriptor")
			binding.descriptorType = HAL::DescriptorType::ReadOnlyBuffer;
		else if (xjsonProperty.value.typeAnnotation == "read-write-buffer-descriptor")
			binding.descriptorType = HAL::DescriptorType::ReadWriteBuffer;
		else if (xjsonProperty.value.typeAnnotation == "read-only-texture-descriptor")
			binding.descriptorType = HAL::DescriptorType::ReadOnlyTexture;
		else if (xjsonProperty.value.typeAnnotation == "read-write-texture-descriptor")
			binding.descriptorType = HAL::DescriptorType::ReadWriteTexture;
		else if (xjsonProperty.value.typeAnnotation == "raytracing-acceleration-structure-descriptor")
			binding.descriptorType = HAL::DescriptorType::RaytracingAccelerationStructure;
		else
		{
			reportError("unknown descriptor type", xjsonProperty.value.typeAnnotationLocation);
			return false;
		}

		if (bindings.isFull())
		{
			reportError("descriptors limit exceeded", xjsonEntryDeclarationProperty.keyLocation);
			return false;
		}

		bindings.pushBack(binding);
	}
	xjsonParser.closeObjectValueScope();

	const uint64 descriptorSetLayoutNameXSH = XSH::Compute(xjsonEntryDeclarationProperty.key);
	if (!descriptorSetLayoutNameXSH)
	{
		reportError("descriptor set layout name XSH = 0. This is not allowed", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}
	if (libraryDefinition.descriptorSetLayouts.find(descriptorSetLayoutNameXSH))
	{
		// TODO: We may give separate error for hash collision (that will never happen).
		reportError("descriptor set layout redefinition", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}

	const HAL::ShaderCompiler::DescriptorSetLayoutRef descriptorSetLayout =
		HAL::ShaderCompiler::DescriptorSetLayout::Create(bindings, bindings.getSize());

	libraryDefinition.descriptorSetLayouts.insert(descriptorSetLayoutNameXSH, descriptorSetLayout);

	return true;
}

bool LibraryDefinitionLoader::readPipelineLayoutDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty)
{
	if (xjsonEntryDeclarationProperty.value.valueType != XJSON::ValueType::Object)
	{
		reportError("pipeline layout declaration property value should be XJSON object",
			xjsonEntryDeclarationProperty.value.valueLocation);
		return false;
	}

	InplaceArrayList<HAL::ShaderCompiler::PipelineBindingDesc, HAL::MaxPipelineBindingCount> bindings;

	xjsonParser.openObjectValueScope();
	for (;;)
	{
		XJSON::KeyValue xjsonProperty = {};
		XJSON::ParserError xjsonError = {};
		const XJSON::ReadStatus xjsonReadStatus = xjsonParser.readKeyValue(xjsonProperty, xjsonError);

		if (xjsonReadStatus == XJSON::ReadStatus::EndOfScope)
			break;
		if (xjsonReadStatus == XJSON::ReadStatus::Error)
		{
			reportXJSONError(xjsonError);
			return false;
		}
		XAssert(xjsonReadStatus == XJSON::ReadStatus::Success);

		if (xjsonProperty.key.isEmpty())
		{
			reportError("pipeline binding name can not be empty", xjsonProperty.keyLocation);
			return false;
		}
		if (xjsonProperty.value.typeAnnotation.isEmpty())
		{
			reportError("pipeline binding type not specified", xjsonProperty.keyLocation);
			return false;
		}
		if (xjsonProperty.value.valueType != XJSON::ValueType::None)
		{
			reportError("value should be empty", xjsonProperty.value.valueLocation);
			return false;
		}

		HAL::ShaderCompiler::PipelineBindingDesc binding = {};
		binding.name = xjsonProperty.key;

		constexpr StringViewASCII descriptorSetTypeLiteral = StringViewASCII("descriptor-set");

		if (xjsonProperty.value.typeAnnotation == "constant-buffer")
			binding.type = HAL::PipelineBindingType::ConstantBuffer;
		else if (xjsonProperty.value.typeAnnotation == "read-only-buffer")
			binding.type = HAL::PipelineBindingType::ReadOnlyBuffer;
		else if (xjsonProperty.value.typeAnnotation == "read-write-buffer")
			binding.type = HAL::PipelineBindingType::ReadWriteBuffer;
		else if (xjsonProperty.value.typeAnnotation.startsWith(descriptorSetTypeLiteral))
		{
			const StringViewASCII descriptorSetLayoutNameInBrackets =
				xjsonProperty.value.typeAnnotation.getSubString(descriptorSetTypeLiteral.getLength());

			if (descriptorSetLayoutNameInBrackets.getLength() < 3 ||
				!descriptorSetLayoutNameInBrackets.startsWith("<") ||
				!descriptorSetLayoutNameInBrackets.endsWith  (">"))
			{
				reportError("expected `descriptor-set<XXX>` syntax", xjsonProperty.value.typeAnnotationLocation);
				return false;
			}

			const StringViewASCII descriptorSetLayoutName =
				descriptorSetLayoutNameInBrackets.getSubString(1, descriptorSetLayoutNameInBrackets.getLength() - 1);
			XAssert(!descriptorSetLayoutName.isEmpty());
			// TODO: Maybe check that `descriptorSetLayoutName` is legal XJSON identifier.

			const HAL::ShaderCompiler::DescriptorSetLayoutRef* descriptorSetLayout =
				libraryDefinition.descriptorSetLayouts.findValue(XSH::Compute(descriptorSetLayoutName));
			if (!descriptorSetLayout)
			{
				reportError("undefined descriptor set layout", xjsonProperty.value.typeAnnotationLocation);
				return false;
			}

			binding.type = HAL::PipelineBindingType::DescriptorSet;
			binding.descriptorSetLayout = descriptorSetLayout->get();
		}
		else
		{
			reportError("unknown pipeline binding type", xjsonProperty.value.typeAnnotationLocation);
			return false;
		}

		if (bindings.isFull())
		{
			reportError("bindings limit exceeded", xjsonEntryDeclarationProperty.keyLocation);
			return false;
		}

		bindings.pushBack(binding);
	}
	xjsonParser.closeObjectValueScope();

	const uint64 pipelineLayoutNameXSH = XSH::Compute(xjsonEntryDeclarationProperty.key);
	if (!pipelineLayoutNameXSH)
	{
		reportError("pipeline layout name XSH = 0. This is not allowed", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}
	if (libraryDefinition.pipelineLayouts.find(pipelineLayoutNameXSH))
	{
		// TODO: We may give separate error for hash collision (that will never happen).
		reportError("pipeline layout redefinition", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}

	const HAL::ShaderCompiler::PipelineLayoutRef pipelineLayout =
		HAL::ShaderCompiler::PipelineLayout::Create(bindings, bindings.getSize());

	libraryDefinition.pipelineLayouts.insert(pipelineLayoutNameXSH, pipelineLayout);

	return true;
}

bool LibraryDefinitionLoader::readGraphicsPipelineDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty)
{
	if (xjsonEntryDeclarationProperty.value.valueType != XJSON::ValueType::Object)
	{
		reportError("graphics pipeline declaration property value should be XJSON object",
			xjsonEntryDeclarationProperty.value.valueLocation);
		return false;
	}

	HAL::ShaderCompiler::PipelineLayoutRef pipelineLayout = nullptr;
	uint64 pipelineLayoutNameXSH = 0;
	HAL::ShaderCompiler::GraphicsPipelineShaders pipelineShaders = {};
	HAL::ShaderCompiler::GraphicsPipelineSettings pipelineSettings = {};
	bool renderTargetsAreSet = false;
	bool depthStencilIsSet = false;

	xjsonParser.openObjectValueScope();
	for (;;)
	{
		XJSON::KeyValue xjsonProperty = {};
		XJSON::ParserError xjsonError = {};
		const XJSON::ReadStatus xjsonReadStatus = xjsonParser.readKeyValue(xjsonProperty, xjsonError);

		if (xjsonReadStatus == XJSON::ReadStatus::EndOfScope)
			break;
		if (xjsonReadStatus == XJSON::ReadStatus::Error)
		{
			reportXJSONError(xjsonError);
			return false;
		}
		XAssert(xjsonReadStatus == XJSON::ReadStatus::Success);

		if (!xjsonProperty.value.typeAnnotation.isEmpty())
		{
			reportError("pipeline setup property should have no type annotation", xjsonProperty.keyLocation);
			return false;
		}

		if (xjsonProperty.key == "pipeline-layout")
		{
			if (pipelineLayout)
			{
				reportError("pipeline layout is already defined", xjsonProperty.keyLocation);
				return false;
			}

			if (!readPipelineLayoutSetupProperty(xjsonProperty, pipelineLayout, pipelineLayoutNameXSH))
				return false;
		}
		else if (xjsonProperty.key == "vs")
		{
			if (!pipelineShaders.vs.sourcePath.isEmpty())
			{
				reportError("vertex shader is already defined", xjsonProperty.keyLocation);
				return false;
			}

			if (!readShaderSetupProperty(xjsonProperty, pipelineShaders.vs))
				return false;
		}
		else if (xjsonProperty.key == "ps")
		{
			if (!pipelineShaders.ps.sourcePath.isEmpty())
			{
				reportError("pixel shader is already defined", xjsonProperty.keyLocation);
				return false;
			}

			if (!readShaderSetupProperty(xjsonProperty, pipelineShaders.ps))
				return false;
		}
		else if (xjsonProperty.key == "render-targets")
		{
			if (renderTargetsAreSet)
			{
				reportError("render targets are already defined", xjsonProperty.keyLocation);
				return false;
			}
			if (xjsonProperty.value.valueType != XJSON::ValueType::Array)
			{
				reportError("render targets setup property value should be array", xjsonProperty.value.valueLocation);
				return false;
			}

			uint8 renderTargetCount = 0;
			xjsonParser.openArrayValueScope();
			for (;;)
			{
				XJSON::Value xjsonRenderTargetValue = {};
				XJSON::ParserError xjsonError = {};
				const XJSON::ReadStatus xjsonReadStatus = xjsonParser.readValue(xjsonRenderTargetValue, xjsonError);

				if (xjsonReadStatus == XJSON::ReadStatus::EndOfScope)
					break;
				if (xjsonReadStatus == XJSON::ReadStatus::Error)
				{
					reportXJSONError(xjsonError);
					return false;
				}
				XAssert(xjsonReadStatus == XJSON::ReadStatus::Success);

				if (!xjsonRenderTargetValue.typeAnnotation.isEmpty())
				{
					reportError("render target value should have no type annotation", xjsonRenderTargetValue.typeAnnotationLocation);
					return false;
				}
				if (xjsonRenderTargetValue.valueType != XJSON::ValueType::Literal)
				{
					reportError("render target value should be literal", xjsonRenderTargetValue.valueLocation);
					return false;
				}

				HAL::TexelViewFormat renderTargetFormat = HAL::TexelViewFormat::Undefined;
				if (!ParseTexelViewFormatString(xjsonRenderTargetValue.valueLiteral, renderTargetFormat))
				{
					reportError("unknown format", xjsonRenderTargetValue.valueLocation);
					return false;
				}
				if (!HAL::DoesTexelViewFormatSupportColorRenderTargetUsage(renderTargetFormat))
				{
					reportError("format does not support render target usage", xjsonRenderTargetValue.valueLocation);
					return false;
				}

				if (renderTargetCount >= HAL::MaxRenderTargetCount)
				{
					reportError("render targets limit exceeded", xjsonProperty.value.valueLocation);
					return false;
				}
				pipelineSettings.renderTargetsFormats[renderTargetCount] = renderTargetFormat;
				renderTargetCount++;
			}
			xjsonParser.closeArrayValueScope();

			renderTargetsAreSet = true;
		}
		else if (xjsonProperty.key == "depth-stencil")
		{
			if (depthStencilIsSet)
			{
				reportError("depth stencil is already defined", xjsonProperty.keyLocation);
				return false;
			}
			if (xjsonProperty.value.valueType != XJSON::ValueType::Literal)
			{
				reportError("depth stencil setup property value should be literal", xjsonProperty.value.valueLocation);
				return false;
			}

			HAL::DepthStencilFormat depthStencilFormat = HAL::DepthStencilFormat::Undefined;
			if (!ParseDepthStencilFormatString(xjsonProperty.value.valueLiteral, depthStencilFormat))
			{
				reportError("unknown format", xjsonProperty.value.valueLocation);
				return false;
			}

			pipelineSettings.depthStencilFormat = depthStencilFormat;
			depthStencilIsSet = true;
		}
		else
		{
			reportError("unknown graphics pipeline setup property", xjsonProperty.keyLocation);
			return false;
		}
	}
	xjsonParser.closeObjectValueScope();

	if (!pipelineLayout)
	{
		reportError("pipeline layout is not defined", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}
	XAssert(pipelineLayoutNameXSH);

	// TODO: More validation (shader combinations etc).

	const uint64 pipelineNameXSH = XSH::Compute(xjsonEntryDeclarationProperty.key);
	if (!pipelineNameXSH)
	{
		reportError("pipeline name XSH = 0. This is not allowed", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}
	if (libraryDefinition.pipelines.find(pipelineNameXSH))
	{
		// TODO: We may give separate error for hash collision (that will never happen).
		reportError("pipeline redefinition", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}

	const PipelineRef pipeline = Pipeline::Create(xjsonEntryDeclarationProperty.key);
	pipeline->pipelineLayout = asRValue(pipelineLayout);
	pipeline->pipelineLayoutNameXSH = pipelineLayoutNameXSH;
	pipeline->graphics.shaders = pipelineShaders;
	pipeline->graphics.settings = pipelineSettings;
	pipeline->isGraphics = true;

	libraryDefinition.pipelines.insert(pipelineNameXSH, pipeline);

	return true;
}

bool LibraryDefinitionLoader::readComputePipelineDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty)
{
	if (xjsonEntryDeclarationProperty.value.valueType != XJSON::ValueType::Object)
	{
		reportError("compute pipeline declaration property value should be XJSON object",
			xjsonEntryDeclarationProperty.value.valueLocation);
		return false;
	}

	HAL::ShaderCompiler::PipelineLayoutRef pipelineLayout = nullptr;
	uint64 pipelineLayoutNameXSH = 0;
	HAL::ShaderCompiler::ShaderDesc computeShader = {};

	xjsonParser.openObjectValueScope();
	for (;;)
	{
		XJSON::KeyValue xjsonProperty = {};
		XJSON::ParserError xjsonError = {};
		const XJSON::ReadStatus xjsonReadStatus = xjsonParser.readKeyValue(xjsonProperty, xjsonError);

		if (xjsonReadStatus == XJSON::ReadStatus::EndOfScope)
			break;
		if (xjsonReadStatus == XJSON::ReadStatus::Error)
		{
			reportXJSONError(xjsonError);
			return false;
		}
		XAssert(xjsonReadStatus == XJSON::ReadStatus::Success);

		if (!xjsonProperty.value.typeAnnotation.isEmpty())
		{
			reportError("pipeline setup property should have no type annotation", xjsonProperty.keyLocation);
			return false;
		}

		if (xjsonProperty.key == "pipeline-layout")
		{
			if (pipelineLayout)
			{
				reportError("pipeline layout is already defined", xjsonProperty.keyLocation);
				return false;
			}

			if (!readPipelineLayoutSetupProperty(xjsonProperty, pipelineLayout, pipelineLayoutNameXSH))
				return false;
		}
		else if (xjsonProperty.key == "cs")
		{
			if (!computeShader.sourcePath.isEmpty())
			{
				reportError("compute shader is already defined", xjsonProperty.keyLocation);
				return false;
			}

			if (!readShaderSetupProperty(xjsonProperty, computeShader))
				return false;
		}
		else
		{
			reportError("unknown compute pipeline setup property", xjsonProperty.keyLocation);
			return false;
		}
	}
	xjsonParser.closeObjectValueScope();

	if (!pipelineLayout)
	{
		reportError("pipeline layout is not defined", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}
	XAssert(pipelineLayoutNameXSH);

	if (computeShader.sourcePath.isEmpty())
	{
		reportError("compute shader is not defined", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}

	const uint64 pipelineNameXSH = XSH::Compute(xjsonEntryDeclarationProperty.key);
	if (!pipelineNameXSH)
	{
		reportError("pipeline name XSH = 0. This is not allowed", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}
	if (libraryDefinition.pipelines.find(pipelineNameXSH))
	{
		// TODO: We may give separate error for hash collision (that will never happen).
		reportError("pipeline redefinition", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}

	PipelineRef pipeline = Pipeline::Create(xjsonEntryDeclarationProperty.key);
	pipeline->pipelineLayout = asRValue(pipelineLayout);
	pipeline->pipelineLayoutNameXSH = pipelineLayoutNameXSH;
	pipeline->compute.shader = computeShader;
	pipeline->isGraphics = false;

	libraryDefinition.pipelines.insert(pipelineNameXSH, pipeline);

	return true;
}

bool LibraryDefinitionLoader::readPipelineLayoutSetupProperty(const XJSON::KeyValue& xjsonOuterProperty,
	HAL::ShaderCompiler::PipelineLayoutRef& resultPipelineLayout, uint64& resultPipelineLayoutNameXSH)
{
	if (xjsonOuterProperty.value.valueType != XJSON::ValueType::Literal)
	{
		reportError("pipeline layout setup property value should be literal", xjsonOuterProperty.value.valueLocation);
		return false;
	}

	const uint64 pipelineLayoutNameXSH = XSH::Compute(xjsonOuterProperty.value.valueLiteral);
	const HAL::ShaderCompiler::PipelineLayoutRef* foundPipelineLayout =
		libraryDefinition.pipelineLayouts.findValue(pipelineLayoutNameXSH);
	if (!foundPipelineLayout)
	{
		reportError("unknown pipeline layout", xjsonOuterProperty.value.valueLocation);
		return false;
	}
	// TODO: Additionally compare actual names to check for hash collision (that will never happen).

	resultPipelineLayout = *foundPipelineLayout;
	resultPipelineLayoutNameXSH = pipelineLayoutNameXSH;
	XAssert(resultPipelineLayout);

	return true;
}

bool LibraryDefinitionLoader::readShaderSetupProperty(const XJSON::KeyValue& xjsonOuterProperty,
	HAL::ShaderCompiler::ShaderDesc& resultShader)
{
	if (xjsonOuterProperty.value.valueType != XJSON::ValueType::Object)
	{
		reportError("shader setup property value should be XJSON object", xjsonOuterProperty.value.valueLocation);
		return false;
	}

	StringViewASCII sourcePath;
	StringViewASCII entryPointName;

	XJSON::Location pathLocation = {};

	xjsonParser.openObjectValueScope();
	for (;;)
	{
		XJSON::KeyValue xjsonProperty = {};
		XJSON::ParserError xjsonError = {};
		const XJSON::ReadStatus xjsonReadStatus = xjsonParser.readKeyValue(xjsonProperty, xjsonError);

		if (xjsonReadStatus == XJSON::ReadStatus::EndOfScope)
			break;
		if (xjsonReadStatus == XJSON::ReadStatus::Error)
		{
			reportXJSONError(xjsonError);
			return false;
		}
		XAssert(xjsonReadStatus == XJSON::ReadStatus::Success);

		if (!xjsonProperty.value.typeAnnotation.isEmpty())
		{
			reportError("shader property should have no type annotation", xjsonProperty.keyLocation);
			return false;
		}

		if (xjsonProperty.key == "P")
		{
			if (!sourcePath.isEmpty())
			{
				reportError("shader path already defined", xjsonProperty.value.valueLocation);
				return false;
			}
			if (xjsonProperty.value.valueType != XJSON::ValueType::Literal)
			{
				reportError("shader path property value should be literal", xjsonProperty.value.valueLocation);
				return false;
			}
			if (xjsonProperty.value.valueLiteral.isEmpty())
			{
				reportError("shader path can not be empty", xjsonProperty.value.valueLocation);
				return false;
			}

			sourcePath = xjsonProperty.value.valueLiteral;
			pathLocation = xjsonProperty.value.valueLocation;
		}
		else if (xjsonProperty.key == "E")
		{
			if (!entryPointName.isEmpty())
			{
				reportError("shader entry point name already defined", xjsonProperty.value.valueLocation);
				return false;
			}
			if (xjsonProperty.value.valueType != XJSON::ValueType::Literal)
			{
				reportError("shader entry point name property value should be literal", xjsonProperty.value.valueLocation);
				return false;
			}
			if (xjsonProperty.value.valueLiteral.isEmpty())
			{
				reportError("shader entry point name can not be empty", xjsonProperty.value.valueLocation);
				return false;
			}
			// TODO: Check if entry point name is valid C identifier.

			entryPointName = xjsonProperty.value.valueLiteral;
		}
		else
		{
			reportError("unknown shader property", xjsonProperty.keyLocation);
			return false;
		}
	}
	xjsonParser.closeObjectValueScope();

	if (sourcePath.isEmpty())
	{
		reportError("shader path is not defined", xjsonOuterProperty.keyLocation);
		return false;
	}

	resultShader = {};
	resultShader.sourcePath = sourcePath;
	resultShader.entryPointName = entryPointName;
	return true;
}

bool LibraryDefinitionLoader::load(const char* xjsonPathCStr)
{
	xjsonPath = xjsonPathCStr;

	DynamicStringASCII text;

	// Read text from file.
	{
		File file;
		file.open(xjsonPath, FileAccessMode::Read, FileOpenMode::OpenExisting);
		if (!file.isInitialized())
			return false;

		// TODO: Check for U32 overflow.
		const uint32 fileSize = uint32(file.getSize());

		text.resizeUnsafe(fileSize);
		file.read(text.getData(), fileSize);
		file.close();
	}

	xjsonParser.initialize(text.getData(), text.getLength());
	xjsonParser.forceOpenRootValueScopeAsObject();

	for (;;)
	{
		XJSON::KeyValue xjsonEntryDeclarationProperty = {};
		XJSON::ParserError xjsonError = {};
		const XJSON::ReadStatus xjsonReadStatus = xjsonParser.readKeyValue(xjsonEntryDeclarationProperty, xjsonError);

		if (xjsonReadStatus == XJSON::ReadStatus::EndOfScope)
			break;
		if (xjsonReadStatus == XJSON::ReadStatus::Error)
		{
			reportXJSONError(xjsonError);
			return false;
		}
		XAssert(xjsonReadStatus == XJSON::ReadStatus::Success);

		if (xjsonEntryDeclarationProperty.key.isEmpty())
		{
			reportError("entry name can not be empty", xjsonEntryDeclarationProperty.keyLocation);
			return false;
		}
		if (xjsonEntryDeclarationProperty.value.typeAnnotation.isEmpty())
		{
			reportError("entry type is not specified", xjsonEntryDeclarationProperty.keyLocation);
			return false;
		}

		bool readDeclarationStatus = false;
		if (xjsonEntryDeclarationProperty.value.typeAnnotation == "descriptor-set-layout")
			readDeclarationStatus = readDescriptorSetLayoutDeclaration(xjsonEntryDeclarationProperty);
		else if (xjsonEntryDeclarationProperty.value.typeAnnotation == "pipeline-layout")
			readDeclarationStatus = readPipelineLayoutDeclaration(xjsonEntryDeclarationProperty);
		else if (xjsonEntryDeclarationProperty.value.typeAnnotation == "graphics-pipeline")
			readDeclarationStatus = readGraphicsPipelineDeclaration(xjsonEntryDeclarationProperty);
		else if (xjsonEntryDeclarationProperty.value.typeAnnotation == "compute-pipeline")
			readDeclarationStatus = readComputePipelineDeclaration(xjsonEntryDeclarationProperty);
		else
		{
			reportError("unknown declaration type", xjsonEntryDeclarationProperty.value.typeAnnotationLocation);
			return false;
		}

		if (!readDeclarationStatus)
			return false;
	}

	return true;
}
