#include <XLib.String.h>
#include <XLib.System.File.h>
#include <XLib.Text.h>

#include <XEngine.XStringHash.h>

#include "XEngine.Render.ShaderLibraryBuilder.LibraryDefinitionLoader.h"

#define IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader) \
	do { if ((jsonReader).getErrorCode() != JSONErrorCode::Success) { this->reportJSONError(); return false; } } while (false)
#define IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(condition, message, jsonCursor) \
	do { if (!(condition)) { this->reportError(message, jsonCursor); return false; } } while (false)
#define IF_FALSE_RETURN_FALSE(condition) \
	do { if (!(condition)) return false; } while (false)

using namespace XLib;
using namespace XEngine;
using namespace XEngine::Render::ShaderLibraryBuilder;


// GfxHAL values parsers ///////////////////////////////////////////////////////////////////////////

static GfxHAL::TexelViewFormat ParseTexelViewFormatString(StringViewASCII string)
{
	if (string == "R8_UNORM")			return GfxHAL::TexelViewFormat::R8_UNORM;
	if (string == "R8_SNORM")			return GfxHAL::TexelViewFormat::R8_SNORM;
	if (string == "R8_UINT")			return GfxHAL::TexelViewFormat::R8_UINT;
	if (string == "R8_SINT")			return GfxHAL::TexelViewFormat::R8_SINT;
	if (string == "R8G8_UNORM")			return GfxHAL::TexelViewFormat::R8G8_UNORM;
	if (string == "R8G8_SNORM")			return GfxHAL::TexelViewFormat::R8G8_SNORM;
	if (string == "R8G8_UINT")			return GfxHAL::TexelViewFormat::R8G8_UINT;
	if (string == "R8G8_SINT")			return GfxHAL::TexelViewFormat::R8G8_SINT;
	if (string == "R8G8B8A8_UNORM")		return GfxHAL::TexelViewFormat::R8G8B8A8_UNORM;
	if (string == "R8G8B8A8_SNORM")		return GfxHAL::TexelViewFormat::R8G8B8A8_SNORM;
	if (string == "R8G8B8A8_UINT")		return GfxHAL::TexelViewFormat::R8G8B8A8_UINT;
	if (string == "R8G8B8A8_SINT")		return GfxHAL::TexelViewFormat::R8G8B8A8_SINT;
	if (string == "R8G8B8A8_SRGB")		return GfxHAL::TexelViewFormat::R8G8B8A8_SRGB;
	if (string == "R16_FLOAT")			return GfxHAL::TexelViewFormat::R16_FLOAT;
	if (string == "R16_UNORM")			return GfxHAL::TexelViewFormat::R16_UNORM;
	if (string == "R16_SNORM")			return GfxHAL::TexelViewFormat::R16_SNORM;
	if (string == "R16_UINT")			return GfxHAL::TexelViewFormat::R16_UINT;
	if (string == "R16_SINT")			return GfxHAL::TexelViewFormat::R16_SINT;
	if (string == "R16G16_FLOAT")		return GfxHAL::TexelViewFormat::R16G16_FLOAT;
	if (string == "R16G16_UNORM")		return GfxHAL::TexelViewFormat::R16G16_UNORM;
	if (string == "R16G16_SNORM")		return GfxHAL::TexelViewFormat::R16G16_SNORM;
	if (string == "R16G16_UINT")		return GfxHAL::TexelViewFormat::R16G16_UINT;
	if (string == "R16G16_SINT")		return GfxHAL::TexelViewFormat::R16G16_SINT;
	if (string == "R16G16B16A16_FLOAT")	return GfxHAL::TexelViewFormat::R16G16B16A16_FLOAT;
	if (string == "R16G16B16A16_UNORM")	return GfxHAL::TexelViewFormat::R16G16B16A16_UNORM;
	if (string == "R16G16B16A16_SNORM")	return GfxHAL::TexelViewFormat::R16G16B16A16_SNORM;
	if (string == "R16G16B16A16_UINT")	return GfxHAL::TexelViewFormat::R16G16B16A16_UINT;
	if (string == "R16G16B16A16_SINT")	return GfxHAL::TexelViewFormat::R16G16B16A16_SINT;
	if (string == "R32_FLOAT")			return GfxHAL::TexelViewFormat::R32_FLOAT;
	if (string == "R32_UNORM")			return GfxHAL::TexelViewFormat::R32_UNORM;
	if (string == "R32_SNORM")			return GfxHAL::TexelViewFormat::R32_SNORM;
	if (string == "R32_UINT")			return GfxHAL::TexelViewFormat::R32_UINT;
	if (string == "R32_SINT")			return GfxHAL::TexelViewFormat::R32_SINT;
	if (string == "R32G32_FLOAT")		return GfxHAL::TexelViewFormat::R32G32_FLOAT;
	if (string == "R32G32_UNORM")		return GfxHAL::TexelViewFormat::R32G32_UNORM;
	if (string == "R32G32_SNORM")		return GfxHAL::TexelViewFormat::R32G32_SNORM;
	if (string == "R32G32_UINT")		return GfxHAL::TexelViewFormat::R32G32_UINT;
	if (string == "R32G32_SINT")		return GfxHAL::TexelViewFormat::R32G32_SINT;
	if (string == "R32G32B32A32_FLOAT")	return GfxHAL::TexelViewFormat::R32G32B32A32_FLOAT;
	if (string == "R32G32B32A32_UNORM")	return GfxHAL::TexelViewFormat::R32G32B32A32_UNORM;
	if (string == "R32G32B32A32_SNORM")	return GfxHAL::TexelViewFormat::R32G32B32A32_SNORM;
	if (string == "R32G32B32A32_UINT")	return GfxHAL::TexelViewFormat::R32G32B32A32_UINT;
	if (string == "R32G32B32A32_SINT")	return GfxHAL::TexelViewFormat::R32G32B32A32_SINT;
	if (string == "R24_UNORM_X8")		return GfxHAL::TexelViewFormat::R24_UNORM_X8;
	if (string == "X24_G8_UINT")		return GfxHAL::TexelViewFormat::X24_G8_UINT;
	if (string == "R32_FLOAT_X8")		return GfxHAL::TexelViewFormat::R32_FLOAT_X8;
	if (string == "X32_G8_UINT")		return GfxHAL::TexelViewFormat::X32_G8_UINT;
	return GfxHAL::TexelViewFormat::Undefined;
}

static GfxHAL::DepthStencilFormat ParseDepthStencilFormatString(StringViewASCII string)
{
	if (string == "D16")	return GfxHAL::DepthStencilFormat::D16;
	if (string == "D32")	return GfxHAL::DepthStencilFormat::D32;
	if (string == "D24S8")	return GfxHAL::DepthStencilFormat::D24S8;
	if (string == "D32S8")	return GfxHAL::DepthStencilFormat::D32S8;
	return GfxHAL::DepthStencilFormat::Undefined;
}

static GfxHAL::ShaderType ParseShaderTypeString(StringViewASCII string)
{
	if (string == "cs") return GfxHAL::ShaderType::Compute;
	if (string == "vs") return GfxHAL::ShaderType::Vertex;
	if (string == "as") return GfxHAL::ShaderType::Amplification;
	if (string == "ms") return GfxHAL::ShaderType::Mesh;
	if (string == "ps") return GfxHAL::ShaderType::Pixel;
	return GfxHAL::ShaderType::Undefined;
}

static GfxHAL::SamplerFilterMode ParseSamplerFilterModeString(StringViewASCII string)
{
	if (string == "min_pnt_mag_pnt_mip_pnt") return GfxHAL::SamplerFilterMode::MinPnt_MagPnt_MipPnt;
	if (string == "min_pnt_mag_pnt_mip_lin") return GfxHAL::SamplerFilterMode::MinPnt_MagPnt_MipLin;
	if (string == "min_pnt_mag_lin_mip_pnt") return GfxHAL::SamplerFilterMode::MinPnt_MagLin_MipPnt;
	if (string == "min_pnt_mag_lin_mip_lin") return GfxHAL::SamplerFilterMode::MinPnt_MagLin_MipLin;
	if (string == "min_lin_mag_pnt_mip_pnt") return GfxHAL::SamplerFilterMode::MinLin_MagPnt_MipPnt;
	if (string == "min_lin_mag_pnt_mip_lin") return GfxHAL::SamplerFilterMode::MinLin_MagPnt_MipLin;
	if (string == "min_lin_mag_lin_mip_pnt") return GfxHAL::SamplerFilterMode::MinLin_MagLin_MipPnt;
	if (string == "min_lin_mag_lin_mip_lin") return GfxHAL::SamplerFilterMode::MinLin_MagLin_MipLin;
	if (string == "point")	return GfxHAL::SamplerFilterMode::MinPnt_MagPnt_MipPnt;
	if (string == "linear")	return GfxHAL::SamplerFilterMode::MinPnt_MagPnt_MipPnt;
	if (string == "aniso") return GfxHAL::SamplerFilterMode::Anisotropic;
	return GfxHAL::SamplerFilterMode::Anisotropic;
}

static GfxHAL::SamplerReductionMode ParseSamplerReductionModeString(StringViewASCII string)
{
	if (string == "weighted_avg")		return GfxHAL::SamplerReductionMode::WeightedAverage;
	if (string == "weighted_avg_cmp")	return GfxHAL::SamplerReductionMode::WeightedAverageOfComparisonResult;
	if (string == "min")				return GfxHAL::SamplerReductionMode::Min;
	if (string == "max")				return GfxHAL::SamplerReductionMode::Max;
	return GfxHAL::SamplerReductionMode::Undefined;
}

static GfxHAL::SamplerAddressMode ParseSamplerAddressModeString(StringViewASCII string)
{
	if (string == "wrap")			return GfxHAL::SamplerAddressMode::Wrap;
	if (string == "mirror")			return GfxHAL::SamplerAddressMode::Mirror;
	if (string == "clamp")			return GfxHAL::SamplerAddressMode::Clamp;
	if (string == "border_zero")	return GfxHAL::SamplerAddressMode::BorderZero;
	return GfxHAL::SamplerAddressMode::Undefined;
}

static GfxHAL::ShaderCompiler::VertexBufferStepRate ParseVertexBufferStepRateString(StringViewASCII string)
{
	if (string == "per_vertex")		return GfxHAL::ShaderCompiler::VertexBufferStepRate::PerVertex;
	if (string == "per_instance")	return GfxHAL::ShaderCompiler::VertexBufferStepRate::PerInstance;
	return GfxHAL::ShaderCompiler::VertexBufferStepRate::Undefined;
}

static GfxHAL::DescriptorType ParseDescriptorTypeString(StringViewASCII string)
{
	if (string == "read_only_buffer_descriptor")					return GfxHAL::DescriptorType::ReadOnlyBuffer;
	if (string == "read_write_buffer_descriptor")					return GfxHAL::DescriptorType::ReadWriteBuffer;
	if (string == "read_only_texture_descriptor")					return GfxHAL::DescriptorType::ReadOnlyTexture;
	if (string == "read_write_texture_descriptor")					return GfxHAL::DescriptorType::ReadWriteTexture;
	if (string == "raytracing_acceleration_structure_descriptor")	return GfxHAL::DescriptorType::RaytracingAccelerationStructure;
	return GfxHAL::DescriptorType::Undefined;
}

static GfxHAL::PipelineBindingType ParsePipelineBindingTypeString(StringViewASCII string)
{
	if (string == "constant_buffer")	return GfxHAL::PipelineBindingType::ConstantBuffer;
	if (string == "read_only_buffer")	return GfxHAL::PipelineBindingType::ReadOnlyBuffer;
	if (string == "read_write_buffer")	return GfxHAL::PipelineBindingType::ReadWriteBuffer;
	if (string == "descriptor_set")		return GfxHAL::PipelineBindingType::DescriptorSet;
	return GfxHAL::PipelineBindingType::Undefined;
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

static bool ValidateBindingName(StringViewASCII name)
{
	if (name.isEmpty())
		return false;
	if (!Char::IsUpper(name[0]) && name[0] != '_')
		return false;

	for (uintptr i = 1; i < name.getLength(); i++)
	{
		const char c = name[i];
		if (!Char::IsUpper(c) && !Char::IsDigit(c) && c != '_')
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

static bool ValidateShaderEntryPointName(StringViewASCII name)
{
	if (name.isEmpty())
		return false;
	if (!Char::IsLetter(name[0]) && name[0] != '_')
		return false;

	for (uintptr i = 1; i < name.getLength(); i++)
	{
		const char c = name[i];
		if (!Char::IsLetterOrDigit(c) && c != '_')
			return false;
	}
	return true;
}


// LibraryDefinition utils /////////////////////////////////////////////////////////////////////////

static GfxHAL::ShaderCompiler::DescriptorSetLayout* LibFindDescriptorSetLayout(LibraryDefinition& lib, uint64 nameXSH)
{
	for (const auto& descriptorSetLayout : lib.descriptorSetLayouts)
	{
		if (descriptorSetLayout.nameXSH == nameXSH)
			return descriptorSetLayout.ref.get();
	}
	return nullptr;
}

static GfxHAL::ShaderCompiler::PipelineLayout* LibFindPipelineLayout(LibraryDefinition& lib, uint64 nameXSH)
{
	for (const auto& pipelineLayout : lib.pipelineLayouts)
	{
		if (pipelineLayout.nameXSH == nameXSH)
			return pipelineLayout.ref.get();
	}
	return nullptr;
}

static Pipeline* LibFindPipeline(LibraryDefinition& lib, uint64 nameXSH)
{
	for (const auto& pipeline : lib.pipelines)
	{
		if (pipeline.nameXSH == nameXSH)
			return pipeline.ref.get();
	}
	return nullptr;
}


// LibraryDefinitionLoader /////////////////////////////////////////////////////////////////////////

void LibraryDefinitionLoader::reportError(const char* message, Cursor jsonCursor)
{
	// TODO: Print absolute path.
	TextWriteFmtStdOut(jsonPath, ':', jsonCursor.lineNumber, ':', jsonCursor.columnNumber,
		": error: ", message, '\n');
}

void LibraryDefinitionLoader::reportJSONError()
{
	const JSONErrorCode jsonError = jsonReader.getErrorCode();
	XAssert(jsonError != JSONErrorCode::Success);

	// TODO: Print absolute path.
	TextWriteFmtStdOut(jsonPath, ':', jsonReader.getLineNumer(), ':', jsonReader.getColumnNumer(),
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
	TextWriteFmt(propertyExpectedErrorMessage, '\'', expectedKey, "' property expected");

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
	TextWriteFmt(propertyExpectedErrorMessage, '\'', expectedKey, "' property expected");

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
	TextWriteFmt(propertyExpectedErrorMessage, '\'', expectedKey, "' property expected");

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
	XTODO("Check duplicate static sampler name");

	GfxHAL::SamplerDesc samplerDesc = {};

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
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.filterMode == GfxHAL::SamplerFilterMode::Undefined, "filter mode already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::String, "string expected", jsonValueCursor);

			samplerDesc.filterMode = ParseSamplerFilterModeString(jsonValue.string.string);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.filterMode != GfxHAL::SamplerFilterMode::Undefined, "invalid filter mode", jsonValueCursor);
		}
		else if (jsonKey.string == "reduction")
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.reductionMode == GfxHAL::SamplerReductionMode::Undefined, "reduction mode already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::String, "string expected", jsonValueCursor);

			samplerDesc.reductionMode = ParseSamplerReductionModeString(jsonValue.string.string);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.reductionMode != GfxHAL::SamplerReductionMode::Undefined, "invalid reduction mode", jsonValueCursor);
		}
		else if (jsonKey.string == "max_anisotropy")
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.filterMode == GfxHAL::SamplerFilterMode::Anisotropic, "filter mode should be set to aniso", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.maxAnisotropy == 0, "max anisotropy already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::Number, "integer 1..16 expected", jsonValueCursor);

			XAssertNotImplemented();
		}
		else if (jsonKey.string == "address")
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.addressModeU == GfxHAL::SamplerAddressMode::Undefined, "address mode already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::String, "string expected", jsonValueCursor);

			samplerDesc.addressModeU = ParseSamplerAddressModeString(jsonValue.string.string);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.addressModeU != GfxHAL::SamplerAddressMode::Undefined, "invalid address mode", jsonValueCursor);
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

	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.filterMode != GfxHAL::SamplerFilterMode::Undefined, "filter mode not set", jsonStaticSamplerNameCursor);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.reductionMode != GfxHAL::SamplerReductionMode::Undefined, "reduction mode not set", jsonStaticSamplerNameCursor);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.addressModeU != GfxHAL::SamplerAddressMode::Undefined, "address mode not set", jsonStaticSamplerNameCursor);

	if (samplerDesc.filterMode == GfxHAL::SamplerFilterMode::Anisotropic)
		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(samplerDesc.maxAnisotropy > 0, "max anisotropy not set", jsonStaticSamplerNameCursor);

	samplerDesc.addressModeV = samplerDesc.addressModeU;
	samplerDesc.addressModeW = samplerDesc.addressModeU;
	samplerDesc.lodBias = 0.0f;
	samplerDesc.lodMin = -100.0f;
	samplerDesc.lodMax = +100.0f;

	XAssertNotImplemented();

	return false;
}

bool LibraryDefinitionLoader::readVertexInputLayout(StringViewASCII vertexInputLayoutName, Cursor jsonVertexInputLayoutNameCursor)
{
	XTODO("Check duplicate vertex inout layout name");
	
	VertexInputLayoutDesc vertexInputLayoutDesc = {};
	vertexInputLayoutDesc.name = vertexInputLayoutName;
	vertexInputLayoutDesc.bindingsOffset = vertexBindings.getSize();
	vertexInputLayoutDesc.bindingCount = 0;

	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithArrayValue("buffers"));

	jsonReader.openArray();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	uint8 vertexBufferCount = 0;
	while (!jsonReader.isEndOfArray())
	{
		JSONValue jsonBufferValue = {};
		const Cursor jsonBufferValueCursor = getJSONCursor();
		jsonReader.readValue(jsonBufferValue);
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonBufferValue.type == JSONValueType::Object, "object expected", jsonBufferValueCursor);

		jsonReader.openObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(vertexBufferCount < GfxHAL::MaxVertexBufferCount, "vertex buffers limit exceeded", jsonBufferValueCursor);
			const uint8 vertexBufferIndex = vertexBufferCount;
			vertexBufferCount++;

			GfxHAL::ShaderCompiler::VertexBufferDesc& vertexBufferDesc = vertexInputLayoutDesc.buffers[vertexBufferIndex];

			StringViewASCII stepRateString = {};
			const Cursor jsonStepRateStringCursor = getJSONCursor();
			IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("step_rate", stepRateString));

			vertexBufferDesc.stepRate = ParseVertexBufferStepRateString(stepRateString);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(vertexBufferDesc.stepRate != GfxHAL::ShaderCompiler::VertexBufferStepRate::Undefined,
				"invalid vertex buffer step rate", jsonStepRateStringCursor);

			IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithArrayValue("bindings"));

			jsonReader.openArray();
			IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

			uint16 vertexBindingDataOffset = 0;
			while (!jsonReader.isEndOfArray())
			{
				JSONValue jsonBindingValue = {};
				const Cursor jsonBindingValueCursor = getJSONCursor();
				jsonReader.readValue(jsonBindingValue);
				IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
				IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonBindingValue.type == JSONValueType::Object, "object expected", jsonBindingValueCursor);

				GfxHAL::ShaderCompiler::VertexBindingDesc vertexBindingDesc = {};

				jsonReader.openObject();
				IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
				{
					StringViewASCII bindingName = {};
					const Cursor jsonBindingNameCursor = getJSONCursor();
					IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("name", bindingName));
					IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(ValidateBindingName(bindingName), "invalid binding name", jsonBindingNameCursor);
					IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(bindingName.getLength() > GfxHAL::MaxVertexBindingNameLength, "binding name is too long", jsonBindingNameCursor);

					StringViewASCII bindingFormatStr = {};
					const Cursor jsonBindingFormatCursor = getJSONCursor();
					IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("format", bindingFormatStr));

					const GfxHAL::TexelViewFormat bindingFormat = ParseTexelViewFormatString(bindingFormatStr);
					IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(bindingFormat != GfxHAL::TexelViewFormat::Undefined, "invalid format", jsonBindingFormatCursor);
					IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(GfxHAL::TexelViewFormatUtils::SupportsVertexInputUsage(bindingFormat),
						"format does not support vertex input usage", jsonBindingFormatCursor);

					vertexBindingDesc.name = bindingName;
					vertexBindingDesc.offset = vertexBindingDataOffset;
					vertexBindingDesc.format = bindingFormat;
					vertexBindingDesc.bufferIndex = vertexBufferIndex;

					XTODO("Add padding");
					vertexBindingDataOffset += GfxHAL::TexelViewFormatUtils::GetByteSize(bindingFormat);
				}
				jsonReader.closeObject();
				IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

				IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(vertexInputLayoutDesc.bindingCount < GfxHAL::MaxVertexBindingCount, "vertex bindings limit exceeded", jsonBindingValueCursor);
				const uint8 localVertexBindingIndex = vertexInputLayoutDesc.bindingCount;
				vertexInputLayoutDesc.bindingCount++;

				vertexBindings.pushBack(vertexBindingDesc);
				XAssert(vertexInputLayoutDesc.bindingsOffset + vertexInputLayoutDesc.bindingCount == vertexBindings.getSize());
			}

			jsonReader.closeArray();
			IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
		}
		jsonReader.closeObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
	}

	jsonReader.closeArray();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	vertexInputLayouts.pushBack(vertexInputLayoutDesc);

	return false;
}

bool LibraryDefinitionLoader::readDescriptorSetLayout(StringViewASCII descriptorSetLayoutName, Cursor jsonDescriptorSetLayoutNameCursor)
{
	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithObjectValue("bindings"));

	InplaceArrayList<GfxHAL::ShaderCompiler::DescriptorSetBindingDesc, GfxHAL::MaxDescriptorSetBindingCount> bindings;

	jsonReader.openObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	while (!jsonReader.isEndOfObject())
	{
		GfxHAL::ShaderCompiler::DescriptorSetBindingDesc binding = {};
		binding.descriptorCount = 1;

		const Cursor jsonBindingNameCursor = getJSONCursor();
		IF_FALSE_RETURN_FALSE(consumeKeyWithObjectValue(binding.name));
		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(ValidateBindingName(binding.name), "invalid binding name", jsonBindingNameCursor);

		jsonReader.openObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
		{
			StringViewASCII descriptorTypeStr = {};
			const Cursor jsonDescriptorTypeCursor = getJSONCursor();
			IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("type", descriptorTypeStr));

			binding.descriptorType = ParseDescriptorTypeString(descriptorTypeStr);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(binding.descriptorType != GfxHAL::DescriptorType::Undefined,
				"invalid descriptor type", jsonDescriptorTypeCursor);

			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonReader.isEndOfObject(), "end of object expected", getJSONCursor());
		}
		jsonReader.closeObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!bindings.isFull(), "bindings limit exceeded", jsonBindingNameCursor);
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

	const GfxHAL::ShaderCompiler::DescriptorSetLayoutRef descriptorSetLayout =
		GfxHAL::ShaderCompiler::DescriptorSetLayout::Create(bindings, bindings.getSize());

	//libraryDefinition.descriptorSetLayouts.insert(descriptorSetLayoutNameXSH, descriptorSetLayout);
	libraryDefinition.descriptorSetLayouts.pushBack(
		LibraryDefinition::DescriptorSetLayout { descriptorSetLayoutNameXSH, descriptorSetLayout });

	return true;
}

bool LibraryDefinitionLoader::readPipelineLayout(StringViewASCII pipelineLayoutName, Cursor jsonPipelineLayoutNameCursor)
{
	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithObjectValue("bindings"));

	InplaceArrayList<GfxHAL::ShaderCompiler::PipelineBindingDesc, GfxHAL::MaxPipelineBindingCount> bindings;

	jsonReader.openObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	while (!jsonReader.isEndOfObject())
	{
		GfxHAL::ShaderCompiler::PipelineBindingDesc binding = {};

		const Cursor jsonBindingNameCursor = getJSONCursor();
		IF_FALSE_RETURN_FALSE(consumeKeyWithObjectValue(binding.name));
		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(ValidateBindingName(binding.name), "invalid binding name", jsonBindingNameCursor);

		jsonReader.openObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
		{
			StringViewASCII bindingTypeStr = {};
			const Cursor jsonBindingTypeCursor = getJSONCursor();
			IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("type", bindingTypeStr));

			binding.type = ParsePipelineBindingTypeString(bindingTypeStr);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(binding.type != GfxHAL::PipelineBindingType::Undefined,
				"invalid pipeline binding type", jsonBindingTypeCursor);

			if (binding.type == GfxHAL::PipelineBindingType::DescriptorSet)
			{
				StringViewASCII descriptorSetLayoutName = {};
				const Cursor jsonDescriptorSetLayoutNameCursor = getJSONCursor();
				IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("dsl", descriptorSetLayoutName));

				//const GfxHAL::ShaderCompiler::DescriptorSetLayoutRef* descriptorSetLayout =
				//	libraryDefinition.descriptorSetLayouts.findValue(XSH::Compute(descriptorSetLayoutName));
				const GfxHAL::ShaderCompiler::DescriptorSetLayout* descriptorSetLayout =
					LibFindDescriptorSetLayout(libraryDefinition, XSH::Compute(descriptorSetLayoutName));

				IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(descriptorSetLayout, "undefined descriptor set layout", jsonDescriptorSetLayoutNameCursor);

				//binding.descriptorSetLayout = descriptorSetLayout->get();
				binding.descriptorSetLayout = descriptorSetLayout;
			}

			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonReader.isEndOfObject(), "end of object expected", getJSONCursor());
		}
		jsonReader.closeObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

		IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!bindings.isFull(), "bindings limit exceeded", jsonBindingNameCursor);
		bindings.pushBack(binding);
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

	const GfxHAL::ShaderCompiler::PipelineLayoutRef pipelineLayout =
		GfxHAL::ShaderCompiler::PipelineLayout::Create(bindings, bindings.getSize(), nullptr, 0);

	//libraryDefinition.pipelineLayouts.insert(pipelineLayoutNameXSH, pipelineLayout);
	libraryDefinition.pipelineLayouts.pushBack(
		LibraryDefinition::PipelineLayout { pipelineLayoutNameXSH, pipelineLayout });

	return true;
}

bool LibraryDefinitionLoader::readGraphicsPipeline(StringViewASCII graphicsPipelineName, Cursor jsonGraphicsPipelineNameCursor)
{
	GfxHAL::ShaderCompiler::PipelineLayoutRef pipelineLayout = nullptr;
	uint64 pipelineLayoutNameXSH = 0;
	GfxHAL::ShaderCompiler::GraphicsPipelineShaders pipelineShaders = {};
	GfxHAL::ShaderCompiler::GraphicsPipelineSettings pipelineSettings = {};
	bool renderTargetsAreSet = false;
	bool depthStencilIsSet = false;

	IF_FALSE_RETURN_FALSE(readPipelineLayoutSetupProperty(pipelineLayout, pipelineLayoutNameXSH));

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

		const GfxHAL::ShaderType shaderType = ParseShaderTypeString(jsonKey.string);
		if (shaderType != GfxHAL::ShaderType::Undefined)
		{
			GfxHAL::ShaderCompiler::ShaderDesc* shader = nullptr;
			switch (shaderType)
			{
				case GfxHAL::ShaderType::Vertex:		shader = &pipelineShaders.vs; break;
				case GfxHAL::ShaderType::Amplification:	shader = &pipelineShaders.as; break;
				case GfxHAL::ShaderType::Mesh:			shader = &pipelineShaders.ms; break;
				case GfxHAL::ShaderType::Pixel:			shader = &pipelineShaders.ps; break;
			}
			XAssert(shader);

			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(shader->sourceText.isEmpty(), "shader already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::Object, "object expected", jsonValueCursor);
			IF_FALSE_RETURN_FALSE(readShaderSetupObject(*shader));
		}
		else if (jsonKey.string == "render_targets")
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!renderTargetsAreSet, "render targets already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::Array, "array expected", jsonValueCursor);

			jsonReader.openArray();
			IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

			uint8 renderTargetCount = 0;
			while (!jsonReader.isEndOfArray())
			{
				JSONValue jsonRTValue = {};
				const Cursor jsonRTValueCursor = getJSONCursor();
				jsonReader.readValue(jsonRTValue);
				IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

				GfxHAL::TexelViewFormat renderTargetFormat = GfxHAL::TexelViewFormat::Undefined;
				if (jsonRTValue.type != JSONValueType::Null)
				{
					IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonRTValue.type == JSONValueType::String, "string or null expected", jsonRTValueCursor);

					renderTargetFormat = ParseTexelViewFormatString(jsonRTValue.string.string);
					IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(renderTargetFormat != GfxHAL::TexelViewFormat::Undefined, "invalid format", jsonRTValueCursor);
					IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(GfxHAL::TexelViewFormatUtils::SupportsRenderTargetUsage(renderTargetFormat),
						"format does not support render target usage", jsonRTValueCursor);
				}

				IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(renderTargetCount < GfxHAL::MaxRenderTargetCount, "render targets limit exceeded", jsonRTValueCursor);

				pipelineSettings.renderTargetsFormats[renderTargetCount] = renderTargetFormat;
				renderTargetCount++;
			}

			jsonReader.closeArray();
			IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

			renderTargetsAreSet = true;
		}
		else if (jsonKey.string == "depth_stencil")
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!depthStencilIsSet, "depth stencil already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::String, "string expected", jsonValueCursor);

			const GfxHAL::DepthStencilFormat depthStencilFormat = ParseDepthStencilFormatString(jsonValue.string.string);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(depthStencilFormat != GfxHAL::DepthStencilFormat::Undefined, "invalid format", jsonValueCursor);

			pipelineSettings.depthStencilFormat = depthStencilFormat;
			depthStencilIsSet = true;
		}
		else
		{
			reportError("invalid graphics pipeline setup property", jsonKeyCursor);
			return false;
		}
	}

	XTODO("Validate shader combination");

	const uint64 pipelineNameXSH = XSH::Compute(graphicsPipelineName);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(pipelineNameXSH != 0,
		"pipeline name XSH = 0. This is not allowed", jsonGraphicsPipelineNameCursor);

	//if (libraryDefinition.pipelines.find(pipelineNameXSH))
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!LibFindPipeline(libraryDefinition, pipelineNameXSH),
		"pipeline redefinition", jsonGraphicsPipelineNameCursor);
	// TODO: We may give separate error for hash collision (that will never happen).

	const PipelineRef pipeline = Pipeline::CreateGraphics(graphicsPipelineName,
		pipelineLayout.get(), pipelineLayoutNameXSH, pipelineShaders, pipelineSettings);

	//libraryDefinition.pipelines.insert(pipelineNameXSH, pipeline);
	libraryDefinition.pipelines.pushBack(LibraryDefinition::Pipeline { pipelineNameXSH, pipeline });

	return true;
}

bool LibraryDefinitionLoader::readComputePipeline(StringViewASCII computePipelineName, Cursor jsonComputePipelineNameCursor)
{
	GfxHAL::ShaderCompiler::PipelineLayoutRef pipelineLayout = nullptr;
	uint64 pipelineLayoutNameXSH = 0;
	GfxHAL::ShaderCompiler::ShaderDesc computeShader = {};

	IF_FALSE_RETURN_FALSE(readPipelineLayoutSetupProperty(pipelineLayout, pipelineLayoutNameXSH));
	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithObjectValue("cs"));
	IF_FALSE_RETURN_FALSE(readShaderSetupObject(computeShader));
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonReader.isEndOfObject(), "end of object expected", getJSONCursor());

	const uint64 pipelineNameXSH = XSH::Compute(computePipelineName);
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(pipelineNameXSH != 0,
		"pipeline name XSH = 0. This is not allowed", jsonComputePipelineNameCursor);
	
	//if (libraryDefinition.pipelines.find(pipelineNameXSH))
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(!LibFindPipeline(libraryDefinition, pipelineNameXSH),
		"pipeline redefinition", jsonComputePipelineNameCursor);
	// TODO: We may give separate error for hash collision (that will never happen).

	const PipelineRef pipeline = Pipeline::CreateCompute(computePipelineName,
		pipelineLayout.get(), pipelineLayoutNameXSH, computeShader);

	//libraryDefinition.pipelines.insert(pipelineNameXSH, pipeline);
	libraryDefinition.pipelines.pushBack(LibraryDefinition::Pipeline { pipelineNameXSH, pipeline });

	return true;
}

bool LibraryDefinitionLoader::readPipelineLayoutSetupProperty(
	GfxHAL::ShaderCompiler::PipelineLayoutRef& resultPipelineLayout, uint64& resultPipelineLayoutNameXSH)
{
	StringViewASCII pipelineLayoutName = {};
	const Cursor jsonPipelineLayoutNameCursor = getJSONCursor();
	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("layout", pipelineLayoutName));

	const uint64 pipelineLayoutNameXSH = XSH::Compute(pipelineLayoutName);

	//const GfxHAL::ShaderCompiler::PipelineLayoutRef* foundPipelineLayout =
	//	libraryDefinition.pipelineLayouts.findValue(pipelineLayoutNameXSH);
	GfxHAL::ShaderCompiler::PipelineLayout* foundPipelineLayout =
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

bool LibraryDefinitionLoader::readShaderSetupObject(GfxHAL::ShaderCompiler::ShaderDesc& resultShader)
{
	jsonReader.openObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	StringViewASCII sourcePath = {};
	StringViewASCII entryPointName = {};

	const Cursor jsonSourcePathCursor = getJSONCursor();
	IF_FALSE_RETURN_FALSE(consumeSpecificKeyWithStringValue("path", sourcePath));
	IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(ValidateShaderSourcePath(sourcePath), "invalid shader source path", jsonSourcePathCursor);

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

		if (jsonKey.string == "entry")
		{
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(entryPointName.isEmpty(), "shader entry point name already set", jsonKeyCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(jsonValue.type == JSONValueType::String, "string expected", jsonValueCursor);
			IF_FALSE_REPORT_MESSAGE_AND_RETURN_FALSE(ValidateShaderEntryPointName(jsonValue.string.string), "invalid shader entry point name", jsonValueCursor);
			entryPointName = jsonValue.string.string;
		}
		else
		{
			reportError("invalid shader setup property", jsonKeyCursor);
			return false;
		}
	}

	jsonReader.closeObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	resultShader = {};
	resultShader.sourcePath = sourcePath;
	resultShader.entryPointName = entryPointName;
	return true;
}

bool LibraryDefinitionLoader::load(const char* jsonPathCStr)
{
	jsonPath = jsonPathCStr;

	DynamicStringASCII text;

	// Read text from file.
	{
		File file;
		file.open(jsonPath, FileAccessMode::Read, FileOpenMode::OpenExisting);
		if (!file.isInitialized())
			return false;

		const uint32 fileSize = XCheckedCastU32(file.getSize());

		text.resizeUnsafe(fileSize);
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
			else if (entryType == "vertex_input_layout")
				readEntryStatus = readVertexInputLayout(entryName, jsonEntryNameCursor);
			else if (entryType == "descriptor_set_layout")
				readEntryStatus = readDescriptorSetLayout(entryName, jsonEntryNameCursor);
			else if (entryType == "pipeline_layout")
				readEntryStatus = readPipelineLayout(entryName, jsonEntryNameCursor);
			else if (entryType == "graphics_pipeline")
				readEntryStatus = readGraphicsPipeline(entryName, jsonEntryNameCursor);
			else if (entryType == "compute_pipeline")
				readEntryStatus = readComputePipeline(entryName, jsonEntryNameCursor);
			else
			{
				reportError("invalid entry type", jsonEntryTypeCursor);
				return false;
			}

			if (!readEntryStatus)
				return false;
		}
		jsonReader.closeObject();
		IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);
	}

	jsonReader.closeObject();
	IF_JSON_ERROR_REPORT_AND_RETURN_FALSE(jsonReader);

	return true;
}
