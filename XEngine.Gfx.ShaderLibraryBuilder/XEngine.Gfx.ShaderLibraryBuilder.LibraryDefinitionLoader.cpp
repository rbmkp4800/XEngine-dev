#include <XLib.Fmt.h>
#include <XLib.String.h>
#include <XLib.System.File.h>

#include <XEngine.XStringHash.h>

#include "XEngine.Gfx.ShaderLibraryBuilder.LibraryDefinitionLoader.h"

#define IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader) \
	do { if ((jsonReader).getErrorCode() != JSONErrorCode::Success) { this->reportJSONError(); return false; } } while (false)
#define IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(condition, message, jsonCursor) \
	do { if (!(condition)) { this->reportError(message, jsonCursor); return false; } } while (false)
#define IF_FALSE_RETURN_FALSE(condition) \
	do { if (!(condition)) return false; } while (false)

using namespace XLib;
using namespace XEngine::Gfx;
using namespace XEngine::Gfx::ShaderLibraryBuilder;


// HAL value parsers ///////////////////////////////////////////////////////////////////////////////

static HAL::SamplerFilterMode ParseSamplerFilterModeString(StringViewASCII string)
{
	if (string == "min_pnt_mag_pnt_mip_pnt") return HAL::SamplerFilterMode::MinPnt_MagPnt_MipPnt;
	if (string == "min_pnt_mag_pnt_mip_lin") return HAL::SamplerFilterMode::MinPnt_MagPnt_MipLin;
	if (string == "min_pnt_mag_lin_mip_pnt") return HAL::SamplerFilterMode::MinPnt_MagLin_MipPnt;
	if (string == "min_pnt_mag_lin_mip_lin") return HAL::SamplerFilterMode::MinPnt_MagLin_MipLin;
	if (string == "min_lin_mag_pnt_mip_pnt") return HAL::SamplerFilterMode::MinLin_MagPnt_MipPnt;
	if (string == "min_lin_mag_pnt_mip_lin") return HAL::SamplerFilterMode::MinLin_MagPnt_MipLin;
	if (string == "min_lin_mag_lin_mip_pnt") return HAL::SamplerFilterMode::MinLin_MagLin_MipPnt;
	if (string == "min_lin_mag_lin_mip_lin") return HAL::SamplerFilterMode::MinLin_MagLin_MipLin;
	if (string == "point")	return HAL::SamplerFilterMode::MinPnt_MagPnt_MipPnt;
	if (string == "linear")	return HAL::SamplerFilterMode::MinPnt_MagPnt_MipPnt;
	if (string == "aniso")	return HAL::SamplerFilterMode::Anisotropic;
	return HAL::SamplerFilterMode::Anisotropic;
}

static HAL::SamplerReductionMode ParseSamplerReductionModeString(StringViewASCII string)
{
	if (string == "weighted_avg")		return HAL::SamplerReductionMode::WeightedAverage;
	if (string == "weighted_avg_cmp")	return HAL::SamplerReductionMode::WeightedAverageOfComparisonResult;
	if (string == "min")				return HAL::SamplerReductionMode::Min;
	if (string == "max")				return HAL::SamplerReductionMode::Max;
	return HAL::SamplerReductionMode::Undefined;
}

static HAL::SamplerAddressMode ParseSamplerAddressModeString(StringViewASCII string)
{
	if (string == "wrap")			return HAL::SamplerAddressMode::Wrap;
	if (string == "mirror")			return HAL::SamplerAddressMode::Mirror;
	if (string == "clamp")			return HAL::SamplerAddressMode::Clamp;
	if (string == "border_zero")	return HAL::SamplerAddressMode::BorderZero;
	return HAL::SamplerAddressMode::Undefined;
}

static HAL::DescriptorType ParseDescriptorTypeString(StringViewASCII string)
{
	if (string == "read_only_buffer_descriptor")					return HAL::DescriptorType::ReadOnlyBuffer;
	if (string == "read_write_buffer_descriptor")					return HAL::DescriptorType::ReadWriteBuffer;
	if (string == "read_only_texture_descriptor")					return HAL::DescriptorType::ReadOnlyTexture;
	if (string == "read_write_texture_descriptor")					return HAL::DescriptorType::ReadWriteTexture;
	if (string == "raytracing_acceleration_structure_descriptor")	return HAL::DescriptorType::RaytracingAccelerationStructure;
	return HAL::DescriptorType::Undefined;
}

static HAL::PipelineBindingType ParsePipelineBindingTypeString(StringViewASCII string)
{
	if (string == "constant_buffer")	return HAL::PipelineBindingType::ConstantBuffer;
	if (string == "read_only_buffer")	return HAL::PipelineBindingType::ReadOnlyBuffer;
	if (string == "read_write_buffer")	return HAL::PipelineBindingType::ReadWriteBuffer;
	if (string == "descriptor_set")		return HAL::PipelineBindingType::DescriptorSet;
	return HAL::PipelineBindingType::Undefined;
}


// JSON string validators //////////////////////////////////////////////////////////////////////////

static bool ValidateRootEntryName(StringViewASCII name)
{
	if (name.isEmpty())
		return false;
	if (!Char::IsLetter(name[0]) && name[0] != '_')
		return false;

	for (uintptr i = 1; i < name.getLength(); i++)
	{
		const char c = name[i];
		if (!Char::IsLetterOrDigit(c) && c != '_' && c != '.')
			return false;
	}
	return true;
}

static bool ValidateShaderSourcePath(StringViewASCII path)
{
	if (path.isEmpty())
		return false;
	//if (!Path::HasFileName(path))
	//	return false;

	// TODO: Add some actual validation here.

	return true;
}


// LibraryDefinition utils /////////////////////////////////////////////////////////////////////////

static HAL::ShaderCompiler::DescriptorSetLayout* LibFindDescriptorSetLayout(LibraryDefinition& lib, uint64 nameXSH)
{
	for (const auto& descriptorSetLayout : lib.descriptorSetLayouts)
	{
		if (descriptorSetLayout.nameXSH == nameXSH)
			return descriptorSetLayout.ref.get();
	}
	return nullptr;
}

static HAL::ShaderCompiler::PipelineLayout* LibFindPipelineLayout(LibraryDefinition& lib, uint64 nameXSH)
{
	for (const auto& pipelineLayout : lib.pipelineLayouts)
	{
		if (pipelineLayout.nameXSH == nameXSH)
			return pipelineLayout.ref.get();
	}
	return nullptr;
}

static Shader* LibFindShader(LibraryDefinition& lib, uint64 nameXSH)
{
	for (const ShaderRef& shader : lib.shaders)
	{
		if (shader->getNameXSH() == nameXSH)
			return shader.get();
	}
	return nullptr;
}


// LibraryDefinitionLoader /////////////////////////////////////////////////////////////////////////

void LibraryDefinitionLoader::reportError(const char* message, Cursor jsonCursor)
{
	// TODO: Print absolute path.
	FmtPrintStdOut(jsonPath, ':', jsonCursor.lineNumber, ':', jsonCursor.columnNumber,
		": error: ", message, '\n');
}

void LibraryDefinitionLoader::reportJSONError()
{
	const JSONErrorCode jsonError = jsonReader.getErrorCode();
	XAssert(jsonError != JSONErrorCode::Success);

	// TODO: Print absolute path.
	FmtPrintStdOut(jsonPath, ':', jsonReader.getLineNumer(), ':', jsonReader.getColumnNumer(),
		": error: JSON: ", JSONErrorCodeToString(jsonError), '\n');
}

bool LibraryDefinitionLoader::consumeKeyWithObjectValue(StringViewASCII& resultKey)
{
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!jsonReader.isEndOfObject(), "property expected", getJSONCursor());

	JSONString jsonKey = {};
	jsonReader.readKey(jsonKey);
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	JSONValue jsonValue = {};
	const Cursor jsonValueCursor = getJSONCursor();
	jsonReader.readValue(jsonValue);
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::Object, "object expected", jsonValueCursor);

	resultKey = jsonKey.string;
	return true;
}

bool LibraryDefinitionLoader::consumeSpecificKeyWithStringValue(const char* expectedKey, StringViewASCII& resultStringValue)
{
	InplaceStringASCIIx128 propertyExpectedErrorMessage;
	FmtPrintStr(propertyExpectedErrorMessage, '\'', expectedKey, "' property expected");

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!jsonReader.isEndOfObject(), propertyExpectedErrorMessage.getCStr(), getJSONCursor());

	JSONString jsonKey = {};
	const Cursor jsonKeyCursor = getJSONCursor();
	jsonReader.readKey(jsonKey);
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	JSONValue jsonValue = {};
	const Cursor jsonValueCursor = getJSONCursor();
	jsonReader.readValue(jsonValue);
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonKey.string == expectedKey, propertyExpectedErrorMessage.getCStr(), jsonKeyCursor);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::String, "string expected", jsonValueCursor);

	resultStringValue = jsonValue.string.string;
	return true;
}

bool LibraryDefinitionLoader::consumeSpecificKeyWithObjectValue(const char* expectedKey)
{
	InplaceStringASCIIx128 propertyExpectedErrorMessage;
	FmtPrintStr(propertyExpectedErrorMessage, '\'', expectedKey, "' property expected");

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!jsonReader.isEndOfObject(), propertyExpectedErrorMessage.getCStr(), getJSONCursor());

	JSONString jsonKey = {};
	const Cursor jsonKeyCursor = getJSONCursor();
	jsonReader.readKey(jsonKey);
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	JSONValue jsonValue = {};
	const Cursor jsonValueCursor = getJSONCursor();
	jsonReader.readValue(jsonValue);
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonKey.string == expectedKey, propertyExpectedErrorMessage.getCStr(), jsonKeyCursor);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::Object, "object expected", jsonValueCursor);

	return true;
}

bool LibraryDefinitionLoader::consumeSpecificKeyWithArrayValue(const char* expectedKey)
{
	InplaceStringASCIIx128 propertyExpectedErrorMessage;
	FmtPrintStr(propertyExpectedErrorMessage, '\'', expectedKey, "' property expected");

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!jsonReader.isEndOfObject(), propertyExpectedErrorMessage.getCStr(), getJSONCursor());

	JSONString jsonKey = {};
	const Cursor jsonKeyCursor = getJSONCursor();
	jsonReader.readKey(jsonKey);
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	JSONValue jsonValue = {};
	const Cursor jsonValueCursor = getJSONCursor();
	jsonReader.readValue(jsonValue);
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonKey.string == expectedKey, propertyExpectedErrorMessage.getCStr(), jsonKeyCursor);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::Array, "array expected", jsonValueCursor);

	return true;
}

bool LibraryDefinitionLoader::readStaticSampler(StringViewASCII staticSamplerName, Cursor jsonStaticSamplerNameCursor)
{
	for (StaticSamplerDesc& i : staticSamplers)
	{
		if (i.name == staticSamplerName)
		{
			reportError("static sampler redefinition", jsonStaticSamplerNameCursor);
			return false;
		}
	}

	StaticSamplerDesc samplerDesc = {};
	samplerDesc.name = staticSamplerName;

	while (!jsonReader.isEndOfObject())
	{
		JSONString jsonKey = {};
		const Cursor jsonKeyCursor = getJSONCursor();
		jsonReader.readKey(jsonKey);
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

		JSONValue jsonValue = {};
		const Cursor jsonValueCursor = getJSONCursor();
		jsonReader.readValue(jsonValue);
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

		if (jsonKey.string == "filter")
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.filterMode == HAL::SamplerFilterMode::Undefined, "filter mode already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::String, "string expected", jsonValueCursor);

			samplerDesc.desc.filterMode = ParseSamplerFilterModeString(jsonValue.string.string);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.filterMode != HAL::SamplerFilterMode::Undefined, "invalid filter mode", jsonValueCursor);
		}
		else if (jsonKey.string == "reduction")
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.reductionMode == HAL::SamplerReductionMode::Undefined, "reduction mode already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::String, "string expected", jsonValueCursor);

			samplerDesc.desc.reductionMode = ParseSamplerReductionModeString(jsonValue.string.string);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.reductionMode != HAL::SamplerReductionMode::Undefined, "invalid reduction mode", jsonValueCursor);
		}
		else if (jsonKey.string == "max_anisotropy")
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.filterMode == HAL::SamplerFilterMode::Anisotropic, "filter mode should be set to aniso", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.maxAnisotropy == 0, "max anisotropy already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::Number, "integer 1..16 expected", jsonValueCursor);

			XAssertNotImplemented();
		}
		else if (jsonKey.string == "address")
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.addressModeU == HAL::SamplerAddressMode::Undefined, "address mode already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::String, "string expected", jsonValueCursor);

			samplerDesc.desc.addressModeU = ParseSamplerAddressModeString(jsonValue.string.string);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.addressModeU != HAL::SamplerAddressMode::Undefined, "invalid address mode", jsonValueCursor);
		}
		else if (jsonKey.string == "lod_bias")
		{
			XAssertNotImplemented();
		}
		else if (jsonKey.string == "lod_min")
		{
			XAssertNotImplemented();
		}
		else if (jsonKey.string == "lod_max")
		{
			XAssertNotImplemented();
		}
		else
		{
			reportError("invalid sampler property", jsonKeyCursor);
			return false;
		}
	}

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.filterMode != HAL::SamplerFilterMode::Undefined, "filter mode not set", jsonStaticSamplerNameCursor);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.reductionMode != HAL::SamplerReductionMode::Undefined, "reduction mode not set", jsonStaticSamplerNameCursor);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.addressModeU != HAL::SamplerAddressMode::Undefined, "address mode not set", jsonStaticSamplerNameCursor);

	if (samplerDesc.desc.filterMode == HAL::SamplerFilterMode::Anisotropic)
		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.desc.maxAnisotropy > 0, "max anisotropy not set", jsonStaticSamplerNameCursor);

	samplerDesc.desc.addressModeV = samplerDesc.desc.addressModeU;
	samplerDesc.desc.addressModeW = samplerDesc.desc.addressModeU;
	samplerDesc.desc.lodBias = 0.0f;
	samplerDesc.desc.lodMin = -100.0f;
	samplerDesc.desc.lodMax = +100.0f;

	staticSamplers.pushBack(samplerDesc);

	return true;
}

bool LibraryDefinitionLoader::readDescriptorSetLayout(StringViewASCII descriptorSetLayoutName, Cursor jsonDescriptorSetLayoutNameCursor)
{
	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithObjectValue("bindings"));

	InplaceArrayList<HAL::ShaderCompiler::DescriptorSetBindingDesc, HAL::MaxDescriptorSetBindingCount> bindings;

	jsonReader.openObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	while (!jsonReader.isEndOfObject())
	{
		HAL::ShaderCompiler::DescriptorSetBindingDesc binding = {};
		binding.descriptorCount = 1;

		const Cursor jsonBindingNameCursor = getJSONCursor();
		IF_FALSE_RETURN_FALSE(consumeKeyWithObjectValue(binding.name));
		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(HAL::ShaderCompiler::ValidateDescriptorSetBindingName(binding.name), "invalid binding name", jsonBindingNameCursor);

		jsonReader.openObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
		{
			StringViewASCII descriptorTypeStr = {};
			const Cursor jsonDescriptorTypeCursor = getJSONCursor();
			IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("type", descriptorTypeStr));

			binding.descriptorType = ParseDescriptorTypeString(descriptorTypeStr);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(binding.descriptorType != HAL::DescriptorType::Undefined,
				"invalid descriptor type", jsonDescriptorTypeCursor);

			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonReader.isEndOfObject(), "end of object expected", getJSONCursor());
		}
		jsonReader.closeObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!bindings.isFull(), "descriptor set layout bindings limit exceeded", jsonBindingNameCursor);
		bindings.pushBack(binding);
	}

	jsonReader.closeObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonReader.isEndOfObject(), "end of object expected", getJSONCursor());

	const uint64 descriptorSetLayoutNameXSH = XSH::Compute(descriptorSetLayoutName);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(descriptorSetLayoutNameXSH != 0,
		"descriptor set layout name XSH = 0. This is not allowed", jsonDescriptorSetLayoutNameCursor);

	//if (libraryDefinition.descriptorSetLayouts.find(descriptorSetLayoutNameXSH))
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!LibFindDescriptorSetLayout(libraryDefinition, descriptorSetLayoutNameXSH),
		"descriptor set layout redefinition", jsonDescriptorSetLayoutNameCursor);
	// TODO: We may give separate error for hash collision (that will never happen).

	HAL::ShaderCompiler::GenericErrorMessage objectCreationErrorMessage;

	const HAL::ShaderCompiler::DescriptorSetLayoutRef descriptorSetLayout =
		HAL::ShaderCompiler::DescriptorSetLayout::Create(bindings, bindings.getSize(), objectCreationErrorMessage);

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(descriptorSetLayout, objectCreationErrorMessage.text.getCStr(), jsonDescriptorSetLayoutNameCursor);

	//libraryDefinition.descriptorSetLayouts.insert(descriptorSetLayoutNameXSH, descriptorSetLayout);
	libraryDefinition.descriptorSetLayouts.pushBack(
		LibraryDefinition::DescriptorSetLayout { descriptorSetLayoutNameXSH, descriptorSetLayout });

	return true;
}

bool LibraryDefinitionLoader::readPipelineLayout(StringViewASCII pipelineLayoutName, Cursor jsonPipelineLayoutNameCursor)
{
	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithObjectValue("bindings"));

	InplaceArrayList<HAL::ShaderCompiler::PipelineBindingDesc, HAL::MaxPipelineBindingCount> bindings;
	InplaceArrayList<HAL::ShaderCompiler::StaticSamplerDesc, HAL::ShaderCompiler::MaxPipelineStaticSamplerCount> pipelineLayoutStaticSamplers;

	jsonReader.openObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	while (!jsonReader.isEndOfObject())
	{
		StringViewASCII bindingName = {};
		const Cursor jsonBindingNameCursor = getJSONCursor();
		IF_FALSE_RETURN_FALSE(consumeKeyWithObjectValue(bindingName));
		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(HAL::ShaderCompiler::ValidatePipelineBindingName(bindingName), "invalid binding name", jsonBindingNameCursor);

		jsonReader.openObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
		{
			StringViewASCII bindingTypeStr = {};
			const Cursor jsonBindingTypeCursor = getJSONCursor();
			IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("type", bindingTypeStr));

			if (bindingTypeStr == "static_sampler") // NOTE: Static sampler is not real pipeline binding.
			{
				StringViewASCII staticSamplerName = {};
				const Cursor jsonStaticSamplerNameCursor = getJSONCursor();
				IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("static_sampler", staticSamplerName));

				const StaticSamplerDesc* internalStaticSampler = nullptr;
				for (StaticSamplerDesc& i : staticSamplers)
				{
					if (staticSamplerName == i.name)
					{
						internalStaticSampler = &i;
						break;
					}
				}
				IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(internalStaticSampler, "undefined static sampler", jsonStaticSamplerNameCursor);

				HAL::ShaderCompiler::StaticSamplerDesc staticSampler = {};
				staticSampler.bindingName = bindingName;
				staticSampler.desc = internalStaticSampler->desc;

				IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!pipelineLayoutStaticSamplers.isFull(), "pipeline layout static samplers limit exceeded", jsonBindingNameCursor);
				pipelineLayoutStaticSamplers.pushBack(staticSampler);
			}
			else
			{
				HAL::ShaderCompiler::PipelineBindingDesc binding = {};
				binding.name = bindingName;
				binding.type = ParsePipelineBindingTypeString(bindingTypeStr);
				IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(binding.type != HAL::PipelineBindingType::Undefined,
					"invalid pipeline binding type", jsonBindingTypeCursor);

				if (binding.type == HAL::PipelineBindingType::DescriptorSet)
				{
					StringViewASCII descriptorSetLayoutName = {};
					const Cursor jsonDescriptorSetLayoutNameCursor = getJSONCursor();
					IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("dsl", descriptorSetLayoutName));

					//const HAL::ShaderCompiler::DescriptorSetLayoutRef* descriptorSetLayout =
					//	libraryDefinition.descriptorSetLayouts.findValue(XSH::Compute(descriptorSetLayoutName));
					const HAL::ShaderCompiler::DescriptorSetLayout* descriptorSetLayout =
						LibFindDescriptorSetLayout(libraryDefinition, XSH::Compute(descriptorSetLayoutName));

					IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(descriptorSetLayout, "undefined descriptor set layout", jsonDescriptorSetLayoutNameCursor);

					//binding.descriptorSetLayout = descriptorSetLayout->get();
					binding.descriptorSetLayout = descriptorSetLayout;
				}

				IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!bindings.isFull(), "pipeline layout bindings limit exceeded", jsonBindingNameCursor);
				bindings.pushBack(binding);
			}

			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonReader.isEndOfObject(), "end of object expected", getJSONCursor());
		}
		jsonReader.closeObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
	}

	jsonReader.closeObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonReader.isEndOfObject(), "end of object expected", getJSONCursor());

	const uint64 pipelineLayoutNameXSH = XSH::Compute(pipelineLayoutName);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(pipelineLayoutNameXSH != 0,
		"pipeline layout name XSH = 0. This is not allowed", jsonPipelineLayoutNameCursor);

	//if (libraryDefinition.pipelineLayouts.find(pipelineLayoutNameXSH))
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!LibFindPipelineLayout(libraryDefinition, pipelineLayoutNameXSH),
		"pipeline layout redefinition", jsonPipelineLayoutNameCursor);
	// TODO: We may give separate error for hash collision (that will never happen).

	HAL::ShaderCompiler::GenericErrorMessage objectCreationErrorMessage;

	const HAL::ShaderCompiler::PipelineLayoutRef pipelineLayout =
		HAL::ShaderCompiler::PipelineLayout::Create(bindings, bindings.getSize(),
			pipelineLayoutStaticSamplers, pipelineLayoutStaticSamplers.getSize(), objectCreationErrorMessage);

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(pipelineLayout, objectCreationErrorMessage.text.getCStr(), jsonPipelineLayoutNameCursor);

	//libraryDefinition.pipelineLayouts.insert(pipelineLayoutNameXSH, pipelineLayout);
	libraryDefinition.pipelineLayouts.pushBack(
		LibraryDefinition::PipelineLayout { pipelineLayoutNameXSH, pipelineLayout });

	return true;
}

bool LibraryDefinitionLoader::readShader(StringViewASCII shaderName, Cursor jsonShaderNameCursor, HAL::ShaderType shaderType)
{
	HAL::ShaderCompiler::PipelineLayoutRef pipelineLayout = nullptr;
	uint64 pipelineLayoutNameXSH = 0;

	IF_FALSE_RETURN_FALSE(readPipelineLayoutSetupProperty(pipelineLayout, pipelineLayoutNameXSH));

	StringViewASCII mainSourceFilename;
	HAL::ShaderCompiler::ShaderCompilationArgs args = {};
	args.shaderType = shaderType;

	const Cursor jsonSourcePathCursor = getJSONCursor();
	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("src", mainSourceFilename));
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(ValidateShaderSourcePath(mainSourceFilename), "invalid shader source filename", jsonSourcePathCursor);

	const Cursor jsonEntryPointNameCursor = getJSONCursor();
	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("entry_point", args.entryPointName));
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(HAL::ShaderCompiler::ValidateShaderEntryPointName(args.entryPointName), "invalid shader entry point name", jsonEntryPointNameCursor);

	XTODO(__FUNCTION__ ": Rewrite in less constrained way");

	const uint64 shaderNameXSH = XSH::Compute(shaderName);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(shaderNameXSH != 0, "shader name XSH = 0. This is not allowed", jsonShaderNameCursor);

	//if (libraryDefinition.shaders.find(shaderNameXSH))
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!LibFindShader(libraryDefinition, shaderNameXSH), "shader redefinition", jsonShaderNameCursor);
	// TODO: We may give separate error for hash collision (that will never happen).

	const ShaderRef shader = Shader::Create(shaderName, shaderNameXSH, pipelineLayout.get(), pipelineLayoutNameXSH, mainSourceFilename, args);

	XAssert(shader);
	libraryDefinition.shaders.pushBack(shader);

	return true;
}

bool LibraryDefinitionLoader::readPipelineLayoutSetupProperty(
	HAL::ShaderCompiler::PipelineLayoutRef& resultPipelineLayout, uint64& resultPipelineLayoutNameXSH)
{
	StringViewASCII pipelineLayoutName = {};
	const Cursor jsonPipelineLayoutNameCursor = getJSONCursor();
	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("layout", pipelineLayoutName));

	const uint64 pipelineLayoutNameXSH = XSH::Compute(pipelineLayoutName);

	//const HAL::ShaderCompiler::PipelineLayoutRef* foundPipelineLayout =
	//	libraryDefinition.pipelineLayouts.findValue(pipelineLayoutNameXSH);
	HAL::ShaderCompiler::PipelineLayout* foundPipelineLayout =
		LibFindPipelineLayout(libraryDefinition, pipelineLayoutNameXSH);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(foundPipelineLayout != nullptr,
		"undefined pipeline layout", jsonPipelineLayoutNameCursor);
	// TODO: Additionally compare actual names to check for hash collision (that will never happen).

	//resultPipelineLayout = *foundPipelineLayout;
	resultPipelineLayout = foundPipelineLayout;
	resultPipelineLayoutNameXSH = pipelineLayoutNameXSH;
	XAssert(resultPipelineLayout);

	return true;
}

bool LibraryDefinitionLoader::load(const char* jsonPath)
{
	XAssert(jsonPath && jsonPath[0]);
	// TODO: Validate path.

	this->jsonPath = jsonPath;

	DynamicStringASCII text;

	// Read text from file.
	{
		File file;
		file.open(jsonPath, FileAccessMode::Read, FileOpenMode::OpenExisting);
		if (!file.isOpen())
		{
			FmtPrintStdOut("Cannot open library definition file '", jsonPath, "'");
			return false;
		}

		const uint32 fileSize = XCheckedCastU32(file.getSize());
		text.growBufferToFitLength(fileSize);
		text.setLength(fileSize);
		file.read(text.getData(), fileSize);
		file.close();
	}

	jsonReader.openDocument(text.getData(), text.getLength());
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	JSONValue jsonDocumentRootValue = {};
	jsonReader.readValue(jsonDocumentRootValue);
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonDocumentRootValue.type == JSONValueType::Object, "object expected", getJSONCursor());

	jsonReader.openObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	while (!jsonReader.isEndOfObject())
	{
		StringViewASCII entryName = {};
		const Cursor jsonEntryNameCursor = getJSONCursor();
		IF_FALSE_RETURN_FALSE(consumeKeyWithObjectValue(entryName));
		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(ValidateRootEntryName(entryName), "invalid entry name", jsonEntryNameCursor);

		jsonReader.openObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
		{
			StringViewASCII entryType = {};
			const Cursor jsonEntryTypeCursor = getJSONCursor();
			IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("type", entryType));

			bool readEntryStatus = false;
			if (entryType == "static_sampler")
				readEntryStatus = readStaticSampler(entryName, jsonEntryNameCursor);
			else if (entryType == "descriptor_set_layout")
				readEntryStatus = readDescriptorSetLayout(entryName, jsonEntryNameCursor);
			else if (entryType == "pipeline_layout")
				readEntryStatus = readPipelineLayout(entryName, jsonEntryNameCursor);
			else if (entryType == "vs" || entryType == "vertex_shader")
				readEntryStatus = readShader(entryName, jsonEntryNameCursor, HAL::ShaderType::Vertex);
			else if (entryType == "as" || entryType == "amplification_shader")
				readEntryStatus = readShader(entryName, jsonEntryNameCursor, HAL::ShaderType::Amplification);
			else if (entryType == "ms" || entryType == "mesh_shader")
				readEntryStatus = readShader(entryName, jsonEntryNameCursor, HAL::ShaderType::Mesh);
			else if (entryType == "ps" || entryType == "pixel_shader")
				readEntryStatus = readShader(entryName, jsonEntryNameCursor, HAL::ShaderType::Pixel);
			else if (entryType == "cs" || entryType == "compute_shader")
				readEntryStatus = readShader(entryName, jsonEntryNameCursor, HAL::ShaderType::Compute);
			else
			{
				reportError("invalid entry type", jsonEntryTypeCursor);
				return false;
			}

			if (!readEntryStatus)
				return false;
		}

		// All object properties should be consumed at this point by `read*` methods.
		XAssert(jsonReader.isEndOfObject());

		jsonReader.closeObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
	}

	jsonReader.closeObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	return true;
}

bool LibraryDefinitionLoader::Load(LibraryDefinition& libraryDefinition, const char* jsonPath)
{
	LibraryDefinitionLoader loader(libraryDefinition);
	return loader.load(jsonPath);
}
