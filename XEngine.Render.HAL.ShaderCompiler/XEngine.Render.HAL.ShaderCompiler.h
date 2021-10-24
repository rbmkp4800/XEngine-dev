#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

namespace XEngine::Render::HAL::ShaderCompiler
{
	class CompiledDescriptorBundleLayout;

	enum class Platform : uint8
	{
		Undefined = 0,
		D3D12,
		//D3D12X,
		//AGC,
		//Vulkan,
	};

	enum class ShaderType : uint8
	{
		Undefined = 0,
		CS,
		VS,
		AS,
		MS,
		PS,
	};

	enum class RootBindPointType : uint8
	{
		Undefined = 0,
		Constants,
		ConstantBuffer,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		//Descriptor,
		//DescriptorBundle,
		//DescriptorArray,
	};

	enum class DescriptorBindPointType : uint8
	{
		Undefined = 0,
		ReadOnlyBuffer,
		ReadWriteBuffer,
		ReadOnlyTexture2D,
		ReadWriteTexture2D,
		RaytracingAccelerationStructure,
	};

	struct RootBindPointDesc
	{
		const char* name;
		RootBindPointType type;
		uint8 shaderVisibility;
		uint8 constantCount;
		const CompiledDescriptorBundleLayout* descriptorBundleLayout;
	};

	struct DescriptorBindPointDesc
	{
		const char* name;
		DescriptorBindPointType type;
	};

	struct BindingLayoutDesc
	{
		const RootBindPointDesc* bindPoints;
		uint8 bindPointCount;
	};

	struct DescriptorBundleLayoutDesc
	{
		const DescriptorBindPointDesc* bindPoints;
		uint8 bindPointCount;
	};

	class CompiledBindingLayout : public XLib::NonCopyable
	{
	private:


	public:
		const void* getBinaryBlobData() const;
		uint32 getBinaryBlobSize() const;
	};

	class CompiledDescriptorBundleLayout : public XLib::NonCopyable
	{

	};

	class BinaryBlob
	{

	};

	void CompileDescriptorBundleLayout(Platform platform, const char* name, DescriptorBundleLayoutDesc& desc, CompiledDescriptorBundleLayout& result);
	void CompileBindingLayout(Platform platform, const char* name, const BindingLayoutDesc& desc, CompiledBindingLayout& result);
	void CompileShader(Platform platform, ShaderType shaderType, const CompiledBindingLayout& compiledBindingLayout, ...);
	void CompilePipeline(Platform platform);
}
