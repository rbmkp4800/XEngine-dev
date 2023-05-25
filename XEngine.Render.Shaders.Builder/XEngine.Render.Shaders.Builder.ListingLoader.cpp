#include <XLib.String.h>
#include <XLib.System.File.h>

#include "XEngine.Render.Shaders.Builder.ListingLoader.h"

using namespace XLib;
using namespace XEngine::Render;
using namespace XEngine::Render::Shaders::Builder_;

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

void ListingLoader::reportError(const char* message, const XJSON::Location& location)
{
	TextWriteFmtStdOut(path, ':',
		location.lineNumber, ':', location.columnNumber, ": error: ", message, '\n');
}

void ListingLoader::reportXJSONError(const XJSON::ParserError& xjsonError)
{
	TextWriteFmtStdOut(path, ':',
		xjsonError.location.lineNumber, ':', xjsonError.location.columnNumber,
		": error: XJSON: ", xjsonError.message, '\n');
}

bool ListingLoader::readDescriptorSetLayoutDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty)
{
	if (xjsonEntryDeclarationProperty.value.valueType != XJSON::ValueType::Object)
	{
		reportError("descriptor set layout declaration property value should be XJSON object",
			xjsonEntryDeclarationProperty.value.valueLocation);
		return false;
	}

	InplaceArrayList<HAL::ShaderCompiler::DescriptorSetNestedBindingDesc, HAL::MaxDescriptorSetNestedBindingCount> bindings;

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

		HAL::ShaderCompiler::DescriptorSetNestedBindingDesc binding = {};
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
			reportError("too many descriptors", xjsonEntryDeclarationProperty.keyLocation);
			return false;
		}

		bindings.pushBack(binding);
	}
	xjsonParser.closeObjectValueScope();

	const DescriptorSetLayoutCreationResult descriptorSetLayoutCreationResult =
		descriptorSetLayoutList.create(xjsonEntryDeclarationProperty.key, bindings, uint8(bindings.getSize()));

	if (descriptorSetLayoutCreationResult.status != DescriptorSetLayoutCreationStatus::Success)
	{
		// TODO: Proper error handling (hash collision etc).
		reportError("descriptor set layout redefinition", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}

	return true;
}

bool ListingLoader::readPipelineLayoutDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty)
{
	if (xjsonEntryDeclarationProperty.value.valueType != XJSON::ValueType::Object)
	{
		reportError("pipeline layout declaration property value should be XJSON object",
			xjsonEntryDeclarationProperty.value.valueLocation);
		return false;
	}

	InplaceArrayList<PipelineBindingDesc, HAL::MaxPipelineBindingCount> bindings;

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

		PipelineBindingDesc binding = {};
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

			DescriptorSetLayout* descriptorSetLayout = descriptorSetLayoutList.find(descriptorSetLayoutName);
			if (!descriptorSetLayout)
			{
				reportError("undefined descriptor set layout", xjsonProperty.value.typeAnnotationLocation);
				return false;
			}

			binding.type = HAL::PipelineBindingType::DescriptorSet;
			binding.descriptorSetLayout = descriptorSetLayout;
		}
		else
		{
			reportError("unknown pipeline binding type", xjsonProperty.value.typeAnnotationLocation);
			return false;
		}

		if (bindings.isFull())
		{
			reportError("too many bindings", xjsonEntryDeclarationProperty.keyLocation);
			return false;
		}

		bindings.pushBack(binding);
	}
	xjsonParser.closeObjectValueScope();

	const PipelineLayoutCreationResult pipelineLayoutCreationResult =
		pipelineLayoutList.create(xjsonEntryDeclarationProperty.key, bindings, uint8(bindings.getSize()));

	if (pipelineLayoutCreationResult.status != PipelineLayoutCreationStatus::Success)
	{
		// TODO: Proper error handling (hash collision etc).
		reportError("pipeline layout redefinition", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}

	return true;
}

bool ListingLoader::readGraphicsPipelineDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty)
{
	if (xjsonEntryDeclarationProperty.value.valueType != XJSON::ValueType::Object)
	{
		reportError("graphics pipeline declaration property value should be XJSON object",
			xjsonEntryDeclarationProperty.value.valueLocation);
		return false;
	}

	PipelineLayout* pipelineLayout = nullptr;
	GraphicsPipelineDesc pipelineDesc = {};
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

			if (!readPipelineLayoutSetupProperty(xjsonProperty, pipelineLayout))
				return false;
		}
		else if (xjsonProperty.key == "vs")
		{
			if (pipelineDesc.vertexShader)
			{
				reportError("vertex shader is already defined", xjsonProperty.keyLocation);
				return false;
			}
			if (!pipelineLayout)
			{
				reportError("pipeline layout should be defined prior to shaders", xjsonProperty.keyLocation);
				return false;
			}

			if (!readShaderSetupProperty(xjsonProperty, HAL::ShaderCompiler::ShaderType::Vertex, *pipelineLayout, pipelineDesc.vertexShader))
				return false;
		}
		else if (xjsonProperty.key == "ps")
		{
			if (pipelineDesc.pixelShader)
			{
				reportError("pixel shader is already defined", xjsonProperty.keyLocation);
				return false;
			}
			if (!pipelineLayout)
			{
				reportError("pipeline layout should be defined prior to shaders", xjsonProperty.keyLocation);
				return false;
			}

			if (!readShaderSetupProperty(xjsonProperty, HAL::ShaderCompiler::ShaderType::Pixel, *pipelineLayout, pipelineDesc.pixelShader))
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
					reportError("too many render targets", xjsonProperty.value.valueLocation);
					return false;
				}
				pipelineDesc.renderTargetsFormats[renderTargetCount] = renderTargetFormat;
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

			pipelineDesc.depthStencilFormat = depthStencilFormat;
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

	// TODO: More validation (shader combinations etc).

	const PipelineCreationResult pipelineCreationResult =
		pipelineList.create(xjsonEntryDeclarationProperty.key, *pipelineLayout, &pipelineDesc, nullptr);
	if (pipelineCreationResult.status != PipelineCreationStatus::Success)
	{
		// TODO: Proper error handling (hash collision etc).
		reportError("pipeline with this name already defined", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}

	return true;
}

bool ListingLoader::readComputePipelineDeclaration(const XJSON::KeyValue& xjsonEntryDeclarationProperty)
{
	if (xjsonEntryDeclarationProperty.value.valueType != XJSON::ValueType::Object)
	{
		reportError("compute pipeline declaration property value should be XJSON object",
			xjsonEntryDeclarationProperty.value.valueLocation);
		return false;
	}

	PipelineLayout* pipelineLayout = nullptr;
	Shader* computeShader = nullptr;

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

			if (!readPipelineLayoutSetupProperty(xjsonProperty, pipelineLayout))
				return false;
		}
		else if (xjsonProperty.key == "cs")
		{
			if (computeShader)
			{
				reportError("compute shader is already defined", xjsonProperty.keyLocation);
				return false;
			}
			if (!pipelineLayout)
			{
				reportError("pipeline layout should be defined prior to shaders", xjsonProperty.keyLocation);
				return false;
			}

			if (!readShaderSetupProperty(xjsonProperty, HAL::ShaderCompiler::ShaderType::Compute, *pipelineLayout, computeShader))
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

	if (!computeShader)
	{
		reportError("compute shader is not defined", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}

	const PipelineCreationResult pipelineCreationResult =
		pipelineList.create(xjsonEntryDeclarationProperty.key, *pipelineLayout, nullptr, computeShader);
	if (pipelineCreationResult.status != PipelineCreationStatus::Success)
	{
		// TODO: Proper error handling (hash collision etc).
		reportError("pipeline with this name already defined", xjsonEntryDeclarationProperty.keyLocation);
		return false;
	}

	return true;
}

bool ListingLoader::readPipelineLayoutSetupProperty(const XJSON::KeyValue& xjsonOuterProperty, PipelineLayout*& resultPipelineLayout)
{
	if (xjsonOuterProperty.value.valueType != XJSON::ValueType::Literal)
	{
		reportError("pipeline layout setup property value should be literal", xjsonOuterProperty.value.valueLocation);
		return false;
	}

	resultPipelineLayout = pipelineLayoutList.find(xjsonOuterProperty.value.valueLiteral);
	if (!resultPipelineLayout)
	{
		reportError("undefined pipeline layout", xjsonOuterProperty.value.valueLocation);
		return false;
	}

	return true;
}

bool ListingLoader::readShaderSetupProperty(const XJSON::KeyValue& xjsonOuterProperty,
	HAL::ShaderCompiler::ShaderType shaderType, PipelineLayout& pipelineLayout, Shader*& resultShader)
{
	if (xjsonOuterProperty.value.valueType != XJSON::ValueType::Object)
	{
		reportError("shader setup property value should be XJSON object", xjsonOuterProperty.value.valueLocation);
		return false;
	}

	StringViewASCII path;
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
			if (!path.isEmpty())
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

			path = xjsonProperty.value.valueLiteral;
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

	if (path.isEmpty())
	{
		reportError("shader is not defined", xjsonOuterProperty.keyLocation);
		return false;
	}

	const SourceCreationResult sourceCreationResult = sourceCache.findOrCreate(path);

	if (sourceCreationResult.status == SourceCreationStatus::Failure_PathIsTooLong)
	{
		reportError("shader path is too long", pathLocation);
		return false;
	}
	else if (sourceCreationResult.status == SourceCreationStatus::Failure_InvalidPath)
	{
		reportError("shader path is invalid", pathLocation);
		return false;
	}

	XAssert(sourceCreationResult.status == SourceCreationStatus::Success);
	XAssert(sourceCreationResult.source);

	const ShaderCreationResult shaderCreationResult =
		shaderList.findOrCreate(shaderType, *sourceCreationResult.source, entryPointName, pipelineLayout);

	if (shaderCreationResult.status == ShaderCreationStatus::Failure_ShaderTypeMismatch)
	{
		reportError("same shader is already defined with other type", xjsonOuterProperty.keyLocation);
		return false;
	}

	XAssert(shaderCreationResult.status == ShaderCreationStatus::Success);
	XAssert(shaderCreationResult.shader);
	resultShader = shaderCreationResult.shader;
	return true;
}

ListingLoader::ListingLoader(
	DescriptorSetLayoutList& descriptorSetLayoutList,
	PipelineLayoutList& pipelineLayoutList,
	PipelineList& pipelineList,
	ShaderList& shaderList,
	SourceCache& sourceCache)
	:
	descriptorSetLayoutList(descriptorSetLayoutList),
	pipelineLayoutList(pipelineLayoutList),
	pipelineList(pipelineList),
	shaderList(shaderList),
	sourceCache(sourceCache)
{}

bool ListingLoader::load(const char* pathCStr)
{
	path = pathCStr;

	DynamicStringASCII text;

	// Read text from file.
	{
		File file;
		file.open(pathCStr, FileAccessMode::Read, FileOpenMode::OpenExisting);
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
