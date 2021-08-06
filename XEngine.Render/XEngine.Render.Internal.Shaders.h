#pragma once

namespace XEngine::Render::Internal
{
	struct ShaderData
	{
		const void* data;
		unsigned int size;
	};

	class Shaders abstract final
	{
	public:
		static const ShaderData ScreenQuadVS;
		static const ShaderData DepthDeprojectAndDownscalePS;
		static const ShaderData DepthDownscalePS;
		static const ShaderData LightingPassPS;
		static const ShaderData BloomFilterAndDownscalePS;
		static const ShaderData BloomDownscalePS;
		static const ShaderData BloomBlurHorizontalPS;
		static const ShaderData BloomBlurVerticalPS;
		static const ShaderData BloomBlurVerticalAndAccumulatePS;
		static const ShaderData ToneMappingPS;

		static const ShaderData SceneGeometryAS;
		static const ShaderData DefaultMaterialMS;
		static const ShaderData DefaultMaterialPS;

		static const ShaderData UIColorVS;
		static const ShaderData UIColorPS;
		static const ShaderData UIColorAlphaTextureVS;
		static const ShaderData UIColorAlphaTexturePS;

		static const ShaderData DebugWhitePS;
	};
}