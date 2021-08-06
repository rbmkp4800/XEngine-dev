#include <XLib.Color.h>
#include <XLib.Vectors.Math.h>
#include <XLib.Vectors.Arithmetics.h>
#include <XLib.Math.Matrix3x4.h>
#include <XEngine.Render.Device.h>

#include "GameSample1.h"

using namespace XLib;
using namespace XEngine;
using namespace GameSample1;

static constexpr float32 cameraMouseSens = 0.005f;
static constexpr float32 cameraSpeed = 0.1f;

void GenerateClustersData(const float32x3* vertices, uint32 vertexCount,
	const uint8x4* indices, uint8 indexCount, byte* output)
{
	auto encodeClusterDesc = [](uint32 dataRelativeOffsetX64, uint8 vertexCount, uint8 indexCount) -> uint64
	{
		return uint64(dataRelativeOffsetX64) | (uint64(vertexCount) << 22) | (uint64(indexCount) << 32);
	};

	auto encodeIndex = [](uint8x4 i) -> uint32
	{
		return uint32(i.x) | (uint32(i.y) << 7) | (uint32(i.z) << 14);
	};

	uint64 clusterDesc = encodeClusterDesc(1, vertexCount, indexCount);
	*to<uint64*>(output) = clusterDesc;

	float32x4* outputVerticesBlob = to<float32x4*>(output + 64);
	for (uint8 i = 0; i < vertexCount; i++)
		outputVerticesBlob[i].xyz = vertices[i];

	uint32* outputIndicesBlob = to<uint32*>(outputVerticesBlob + vertexCount);
	for (uint8 i = 0; i < indexCount; i++)
		outputIndicesBlob[i] = encodeIndex(indices[i]);
}

void Game::initialize()
{
	Core::Input::AddHandler(this);
	Core::Input::SetCursorState(Core::CursorState::Disabled);

	Render::Device& renderDevice = Core::Engine::GetRenderDevice();

	scene.initialize(renderDevice);
	gBuffer.initialize(renderDevice, { 1600, 900 });

	defaultMaterialShader = renderDevice.createDefaultMaterialShader();
	//defaultMaterial = renderDevice.createMaterialInstance(defaultMaterialShader);

	geometryDataBuffer = renderDevice.allocateBuffer(4096);

	{
		float32x3 vertices[] =
		{
			{ -1.0f, -1.0f, -1.0f  },
			{  1.0f,  1.0f, -1.0f  },
			{  1.0f, -1.0f, -1.0f  },
			{ -1.0f,  1.0f, -1.0f  },
			{ -1.0f, -1.0f,  1.0f },
			{  1.0f,  1.0f,  1.0f },
			{  1.0f, -1.0f,  1.0f },
			{ -1.0f,  1.0f,  1.0f },
			{ -1.0f,  1.0f, -1.0f },
			{  1.0f,  1.0f,  1.0f },
			{ -1.0f,  1.0f,  1.0f },
			{  1.0f,  1.0f, -1.0f },
			{ -1.0f, -1.0f, -1.0f  },
			{  1.0f, -1.0f,  1.0f  },
			{  1.0f, -1.0f, -1.0f  },
			{ -1.0f, -1.0f,  1.0f  },
			{ -1.0f, -1.0f, -1.0f  },
			{ -1.0f,  1.0f,  1.0f  },
			{ -1.0f, -1.0f,  1.0f  },
			{ -1.0f,  1.0f, -1.0f  },
			{  1.0f, -1.0f, -1.0f },
			{  1.0f,  1.0f,  1.0f },
			{  1.0f,  1.0f, -1.0f },
			{  1.0f, -1.0f,  1.0f },
		};

		uint8x4 indices[] =
		{
			{ 0,  1,  2, 0 },
			{ 0,  3,  1, 0 },
			{ 4,  6,  5, 0 },
			{ 4,  5,  7, 0 },
			{ 8, 10,  9, 0 },
			{ 8,  9, 11, 0 },
			{ 12, 14, 13,0 },
			{ 12, 13, 15,0 },
			{ 16, 18, 17,0 },
			{ 16, 17, 19,0 },
			{ 20, 22, 21,0 },
			{ 20, 21, 23,0 },
		};

		/*float32x3 vertices[] =
		{
			{ -1.0f, -1.0f, 0.0f },
			{ +1.0f, -1.0f, 0.0f },
			{ -1.0f, +1.0f, 0.0f },
			{ +1.0f, +1.0f, 0.0f },
		};

		uint8x4 indices[] =
		{
			{ 0, 1, 3, 0 },
			{ 0, 3, 2, 0 },
		};*/

		byte tmpBuff[4096] = {};

		GenerateClustersData(vertices, countof(vertices), indices, countof(indices), tmpBuff);

		renderDevice.uploadBufferBlocking(geometryDataBuffer, tmpBuff, 4096);
	}


	Render::GeometrySectionDesc geometrySectionsDesc = {};
	geometrySectionsDesc.clustersAddress = geometryDataBuffer;
	geometrySectionsDesc.transformsOffset = 0;
	geometrySectionsDesc.material = defaultMaterial;
	geometrySectionsDesc.clusterCount = 1;

	geometrySectionsBundle = renderDevice.createGeometrySectionBundle(&geometrySectionsDesc, 1);

	scene.allocateTransformsBlock(2);
	scene.insertGeometrySectionBundleInstance(geometrySectionsBundle, 0);
	scene.insertGeometrySectionBundleInstance(geometrySectionsBundle, 1);

	//uiBatch.initialize(renderDevice);
	//font.initializeDefault(renderDevice);



	//Render::TransformAddress ta = scene.allocateTransformsBlock(1);
	//scene.updateTransforms(ta, 1, &Matrix3x4::Identity());
	//scene.insertGeometrySectionsBundleInstance(geometrySectionsBundle, ta);

	camera.position = { -13.0f, -7.0f, 10.0f };
	cameraRotation = { 0.0f, -0.7f };
}

void Game::update(float32 timeDelta)
{
	Render::Device& renderDevice = Core::Engine::GetRenderDevice();
	Render::Target& renderTarget = Core::Engine::GetOutputViewRenderTarget(0);

	{
		// TODO: refactor

		float32x3 viewSpaceTranslation = { 0.0f, 0.0f, 0.0f };
		float32 translationStep = cameraSpeed;
		if (Core::Input::IsKeyDown(VirtualKey('A')))
			viewSpaceTranslation.x -= translationStep;
		if (Core::Input::IsKeyDown(VirtualKey('D')))
			viewSpaceTranslation.x += translationStep;

		if (Core::Input::IsKeyDown(VirtualKey('Q')))
			viewSpaceTranslation.y -= translationStep;
		if (Core::Input::IsKeyDown(VirtualKey('E')))
			viewSpaceTranslation.y += translationStep;

		if (Core::Input::IsKeyDown(VirtualKey('W')))
			viewSpaceTranslation.z += translationStep;
		if (Core::Input::IsKeyDown(VirtualKey('S')))
			viewSpaceTranslation.z -= translationStep;

		float32x2 xyFroward = { 0.0f, 0.0f };
		float32x3 forward = VectorMath::SphericalCoords_zZenith_xReference(cameraRotation, xyFroward);
		float32x2 xyRight = VectorMath::NormalLeft(xyFroward); // TODO: fix

		camera.forward = forward;
		camera.position += forward * viewSpaceTranslation.z;
		camera.position.xy += xyRight * viewSpaceTranslation.x;
		camera.position.z += viewSpaceTranslation.y;
	}

	static float qqq = 0;
	qqq += 0.01f;
	scene.updateTransform(0, Matrix3x4::Translation(0, 0, -1) * Matrix3x4::RotationZ(qqq));
	scene.updateTransform(1, Matrix3x4::Translation(0, 0, +1) * Matrix3x4::RotationZ(-qqq));

	renderDevice.graphicsQueueSceneRender(scene, camera, gBuffer, uint16x2(1600, 900), renderTarget);

	/*customDrawBatch.open();
	customDrawBatch.close();
	renderDevice.graphicsQueueCustomDraw(customDrawBatch);*/

	/*uiBatch.beginDraw(renderTarget, {0, 0, 1440, 900});
	{
		UI::TextRenderer textRenderer;
		textRenderer.beginDraw(uiBatch);
		textRenderer.setPosition({ 10.0f, 10.0f });
		textRenderer.setFont(font);
		textRenderer.setColor(0xFFFF00_rgb);
		textRenderer.write("XEngine\nSample text");
		textRenderer.endDraw();
	}
	uiBatch.endDraw(true);*/

	//renderDevice.renderUI(uiBatch);
}

void Game::onMouseMove(sint16x2 delta)
{
	cameraRotation.x += delta.x * 0.005f;
	cameraRotation.y -= delta.y * 0.005f;
}

void Game::onKeyboard(XLib::VirtualKey key, bool state)
{
	if (key == XLib::VirtualKey::Escape && state)
		Core::Engine::Shutdown();
}

void Game::onCloseRequest()
{
	Core::Engine::Shutdown();
}