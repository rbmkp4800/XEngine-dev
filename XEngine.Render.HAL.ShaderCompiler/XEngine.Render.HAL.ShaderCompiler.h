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

	class CompiledBindingLayout
	{
	private:


	public:
		//void* getBytecode();
	};

	class BinaryBlob
	{

	};

	void CompileBindingLayout(Platform platform, const BindingLayoutDesc& desc, CompiledBindingLayout& compiledBindingLayout);
	void CompileShader(Platform platform, ShaderType shaderType, const CompiledBindingLayout& compiledBindingLayout, ...);
	void CompilePipeline(Platform platform);
}
