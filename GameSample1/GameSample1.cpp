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

struct Cluster
{
	const float32x3* vertices;
	const uint8x4* indices;
	uint8 vertexCount;
	uint8 indexCount;
};

uint32 GenerateClustersData(const Cluster* clusters, uint8 clusterCount, byte* output)
{
	struct ClusterDescriptor
	{
		uint64 a;
		uint64 b;
	};

	auto EncodeClusterDescriptor = [](uint32 dataRelativeOffsetX64, uint8 vertexCount, uint8 indexCount) -> ClusterDescriptor
	{
		ClusterDescriptor result = {};
		result.a = uint64(dataRelativeOffsetX64) | (uint64(vertexCount) << 22) | (uint64(indexCount) << 32);
		return result;
	};

	auto EncodeIndex = [](uint8x4 i) -> uint32
	{
		return uint32(i.x) | (uint32(i.y) << 7) | (uint32(i.z) << 14);
	};

	constexpr uint32 vertexIndexBlobAlignment = 64;
	
	const uint32 headClusterVertexIndexBlobOffset64 =
		divRoundUp<uint32>(sizeof(ClusterDescriptor) * clusterCount, vertexIndexBlobAlignment);

	ClusterDescriptor* clusterDescriptorsTable = (ClusterDescriptor*)output;

	uint32 currentClusterVertexIndexBlobOffset64 = headClusterVertexIndexBlobOffset64;

	for (uint8 clusterIdx = 0; clusterIdx < clusterCount; clusterIdx++)
	{
		const Cluster& cluster = clusters[clusterIdx];

		const uint32 vertexIndexBlobOffset64 = currentClusterVertexIndexBlobOffset64;
		clusterDescriptorsTable[clusterIdx] =
			EncodeClusterDescriptor(vertexIndexBlobOffset64, cluster.vertexCount, cluster.indexCount);

		float32x4* outputVerticesBlob = to<float32x4*>(output + vertexIndexBlobOffset64 * vertexIndexBlobAlignment);
		for (uint8 i = 0; i < cluster.vertexCount; i++)
		{
			outputVerticesBlob[i].xyz = cluster.vertices[i];
			outputVerticesBlob[i].w = 0.0f;
		}

		uint32* outputIndicesBlob = to<uint32*>(outputVerticesBlob + cluster.vertexCount);
		for (uint8 i = 0; i < cluster.indexCount; i++)
			outputIndicesBlob[i] = EncodeIndex(cluster.indices[i]);

		const uint32 vertexIndexBlobSize = cluster.vertexCount * sizeof(float32x4) + cluster.indexCount * sizeof(uint32);
		const uint32 vertexIndexBlobSize64 = divRoundUp<uint32>(vertexIndexBlobSize, vertexIndexBlobAlignment);

		currentClusterVertexIndexBlobOffset64 += vertexIndexBlobSize64;
	}

	const uint32 usedBytes = currentClusterVertexIndexBlobOffset64 * 64;

	return usedBytes;
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

	geometryDataBuffer = renderDevice.allocateBuffer(65536);

	uint8 scc = 0;

	{
		float32x3 srcVertices[] =
		{
			{ -1.0f, -1.0f, -1.0f },
			{  1.0f,  1.0f, -1.0f },
			{  1.0f, -1.0f, -1.0f },
			{ -1.0f,  1.0f, -1.0f },
			{ -1.0f, -1.0f,  1.0f },
			{  1.0f,  1.0f,  1.0f },
			{  1.0f, -1.0f,  1.0f },
			{ -1.0f,  1.0f,  1.0f },
			{ -1.0f,  1.0f, -1.0f },
			{  1.0f,  1.0f,  1.0f },
			{ -1.0f,  1.0f,  1.0f },
			{  1.0f,  1.0f, -1.0f },
			{ -1.0f, -1.0f, -1.0f },
			{  1.0f, -1.0f,  1.0f },
			{  1.0f, -1.0f, -1.0f },
			{ -1.0f, -1.0f,  1.0f },
			{ -1.0f, -1.0f, -1.0f },
			{ -1.0f,  1.0f,  1.0f },
			{ -1.0f, -1.0f,  1.0f },
			{ -1.0f,  1.0f, -1.0f },
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

		byte tmpBuff[65536] = {};

		Cluster clusters[64];

		for (uint8 clusterIdx = 0; clusterIdx < countof(clusters); clusterIdx++)
		{
			Cluster& c = clusters[clusterIdx];

			float32x3 offset;
			offset.x = float32((clusterIdx >> 0) & 3) * 2.0f - 3.0f;
			offset.y = float32((clusterIdx >> 2) & 3) * 2.0f - 3.0f;
			offset.z = float32((clusterIdx >> 4) & 3) * 2.0f - 3.0f;

			float32x3* vertices = (float32x3*)
				SystemHeapAllocator::Allocate(countof(srcVertices) * sizeof(float32x3)); // (.-.)

			for (uint8 i = 0; i < countof(srcVertices); i++)
				vertices[i] = offset + srcVertices[i] * 0.7f;

			c.vertices = vertices;
			c.indices = indices;
			c.vertexCount = uint8(countof(srcVertices));
			c.indexCount = uint8(countof(indices));
		}

		scc = countof(clusters);

		const uint32 bufferUsedBytes = GenerateClustersData(clusters, countof(clusters), tmpBuff);

		renderDevice.uploadBufferBlocking(geometryDataBuffer, tmpBuff, bufferUsedBytes);
	}

	Render::GeometrySectionDesc geometrySectionsDesc = {};
	geometrySectionsDesc.clustersAddress = geometryDataBuffer;
	geometrySectionsDesc.transformsOffset = 0;
	geometrySectionsDesc.material = defaultMaterial;
	geometrySectionsDesc.clusterCount = scc;

	geometrySectionsBundle = renderDevice.createGeometrySectionBundle(&geometrySectionsDesc, 1);

	scene.allocateTransforms(2);
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