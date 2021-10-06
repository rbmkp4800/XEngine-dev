#pragma once

#include <XLib.h>

namespace XEngine::Render::HAL::ShaderCompiler
{
	enum class Platform : uint8
	{
		None = 0,
		D3D12,
		//D3D12X,
		//AGC,
		//Vulkan,
	};

	enum class BindPointType : uint8
	{
		None = 0,
		Constants,
		ConstantBuffer,
		ReadOnlyBuffer,
		ReadWriteBuffer,
	};

	enum class ShaderType : uint8
	{
		None = 0,
		CS,
		VS,
		MS,
		AS,
		PS,
	};

	struct BindPointDesc
	{
		const char* name;
		BindPointType type;
		uint8 constantCount;
		uint8 shaderVisibility;
	};

	struct BindingLayoutDesc
	{
		const BindPointDesc* bindPoints;
		uint8 bindPointCount;
	};

	class BinaryBlob
	{

	};

	void CompileBindingLayout(Platform platform, const BindingLayoutDesc& desc, BinaryBlob** result);
	void CompileShader(Platform platform, ShaderType shaderType, BinaryBlob* bindingLayoutBlob, );
	void CompilePipeline(Platform platform);
}
