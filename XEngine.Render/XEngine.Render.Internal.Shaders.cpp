#include <Windows.h>

#include "XEngine.Render.Internal.Shaders.h"

#include "ScreenQuadVS.cso.h"
#include "DepthDeprojectAndDownscalePS.cso.h"
#include "DepthDownscalePS.cso.h"
#include "LightingPassPS.cso.h"
#include "BloomFilterAndDownscalePS.cso.h"
#include "BloomBlurHorizontalPS.cso.h"
#include "BloomBlurVerticalPS.cso.h"
#include "BloomBlurVerticalAndAccumulatePS.cso.h"
#include "BloomDownscalePS.cso.h"
#include "ToneMappingPS.cso.h"

#include "SceneGeometryAS.cso.h"
#include "DefaultMaterialMS.cso.h"
#include "DefaultMaterialPS.cso.h"

#include "UIColorVS.cso.h"
#include "UIColorPS.cso.h"
#include "UIColorAlphaTextureVS.cso.h"
#include "UIColorAlphaTexturePS.cso.h"

#include "DebugWhitePS.cso.h"

using namespace XEngine::Render::Internal;

const ShaderData Shaders::ScreenQuadVS = { ScreenQuadVSData, sizeof(ScreenQuadVSData) };
const ShaderData Shaders::DepthDeprojectAndDownscalePS = { DepthDeprojectAndDownscalePSData, sizeof(DepthDeprojectAndDownscalePSData) };
const ShaderData Shaders::DepthDownscalePS = { DepthDownscalePSData, sizeof(DepthDownscalePSData) };
const ShaderData Shaders::LightingPassPS = { LightingPassPSData, sizeof(LightingPassPSData) };
const ShaderData Shaders::BloomFilterAndDownscalePS = { BloomFilterAndDownscalePSData, sizeof(BloomFilterAndDownscalePSData) };
const ShaderData Shaders::BloomDownscalePS = { BloomDownscalePSData, sizeof(BloomDownscalePSData) };
const ShaderData Shaders::BloomBlurHorizontalPS = { BloomBlurHorizontalPSData, sizeof(BloomBlurHorizontalPSData) };
const ShaderData Shaders::BloomBlurVerticalPS = { BloomBlurVerticalPSData, sizeof(BloomBlurVerticalPSData) };
const ShaderData Shaders::BloomBlurVerticalAndAccumulatePS = { BloomBlurVerticalAndAccumulatePSData, sizeof(BloomBlurVerticalAndAccumulatePSData) };
const ShaderData Shaders::ToneMappingPS = { ToneMappingPSData, sizeof(ToneMappingPSData) };

const ShaderData Shaders::SceneGeometryAS = { SceneGeometryASData, sizeof(SceneGeometryASData) };
const ShaderData Shaders::DefaultMaterialMS = { DefaultMaterialMSData, sizeof(DefaultMaterialMSData) };
const ShaderData Shaders::DefaultMaterialPS = { DefaultMaterialPSData, sizeof(DefaultMaterialPSData) };

const ShaderData Shaders::UIColorVS = { UIColorVSData, sizeof(UIColorVSData) };
const ShaderData Shaders::UIColorPS = { UIColorPSData, sizeof(UIColorPSData) };
const ShaderData Shaders::UIColorAlphaTextureVS = { UIColorAlphaTextureVSData, sizeof(UIColorAlphaTextureVSData) };
const ShaderData Shaders::UIColorAlphaTexturePS = { UIColorAlphaTexturePSData, sizeof(UIColorAlphaTexturePSData) };

const ShaderData Shaders::DebugWhitePS = { DebugWhitePSData, sizeof(DebugWhitePSData) };