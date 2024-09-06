#include <XLib.Vectors.h>

#include "XEngine.Render.GeometryHeap.h"

using namespace XEngine::Gfx;
using namespace XEngine::Render;

GeometryHeap XEngine::Render::GGeometryHeap;

namespace
{
	struct TestVertex
	{
		float32x3 position;
		float32x3 normal;
		float32x3 tangent;
		float32x2 texcoord;
	};

	const TestVertex CubeVertices[] =
	{
		{ { -1.0f, -1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } },
		{ {  1.0f,  1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } },
		{ {  1.0f, -1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } },
		{ { -1.0f,  1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } },

		{ { -1.0f, -1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } },
		{ {  1.0f,  1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } },
		{ {  1.0f, -1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } },
		{ { -1.0f,  1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } },

		{ { -1.0f,  1.0f, -1.0f }, {  0.0f,  1.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f } },
		{ {  1.0f,  1.0f,  1.0f }, {  0.0f,  1.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f } },
		{ { -1.0f,  1.0f,  1.0f }, {  0.0f,  1.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f } },
		{ {  1.0f,  1.0f, -1.0f }, {  0.0f,  1.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f } },

		{ { -1.0f, -1.0f, -1.0f }, {  0.0f, -1.0f,  0.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } },
		{ {  1.0f, -1.0f,  1.0f }, {  0.0f, -1.0f,  0.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } },
		{ {  1.0f, -1.0f, -1.0f }, {  0.0f, -1.0f,  0.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } },
		{ { -1.0f, -1.0f,  1.0f }, {  0.0f, -1.0f,  0.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } },

		{ { -1.0f, -1.0f, -1.0f }, { -1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f } },
		{ { -1.0f,  1.0f,  1.0f }, { -1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f } },
		{ { -1.0f, -1.0f,  1.0f }, { -1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f } },
		{ { -1.0f,  1.0f, -1.0f }, { -1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f } },

		{ {  1.0f, -1.0f, -1.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f } },
		{ {  1.0f,  1.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f } },
		{ {  1.0f,  1.0f, -1.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f } },
		{ {  1.0f, -1.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f } },
	};

	const uint16 CubeIndices[] =
	{
		0,  1,  2,
		0,  3,  1,

		4,  6,  5,
		4,  5,  7,

		8, 10,  9,
		8,  9, 11,

		12, 14, 13,
		12, 13, 15,

		16, 18, 17,
		16, 17, 19,

		20, 22, 21,
		20, 21, 23,
	};

	const TestVertex PlaneVertices[] =
	{
		{ { -1.0f, -1.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f } },
		{ {  1.0f,  1.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f } },
		{ { -1.0f,  1.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } },
		{ {  1.0f, -1.0f,  0.0f }, {  0.0f,  0.0f,  1.0f }, {  1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f } },
	};

	const uint16 PlaneIndices[] =
	{
		0,  1,  2,
		0,  3,  1,
	};
}


void GeometryHeap::initialize(HAL::Device& gfxHwDevice)
{
	XEAssert(!this->gfxHwDevice);
	this->gfxHwDevice = &gfxHwDevice;

	gfxHwGeometryPool = gfxHwDevice.createStagingBuffer(64 * 1024 * 1024, Gfx::HAL::StagingBufferAccessMode::DeviceReadHostWrite);
	mappedGeometryPool = uintptr(gfxHwDevice.getMappedBufferPtr(gfxHwGeometryPool));
}

GeometryHandle GeometryHeap::createTestCube()
{
	XEAssert(entryCount < countOf(entries));
	const uint32 entryIndex = entryCount;
	entryCount++;

	geometryPoolAllocatedSize = alignUp<uint32>(geometryPoolAllocatedSize, 256);
	const uint32 vertexBufferOffset = geometryPoolAllocatedSize;
	geometryPoolAllocatedSize += sizeof(TestVertex) * countOf(CubeVertices);

	geometryPoolAllocatedSize = alignUp<uint32>(geometryPoolAllocatedSize, 256);
	const uint32 indexBufferOffset = geometryPoolAllocatedSize;
	geometryPoolAllocatedSize += sizeof(uint16) * countOf(CubeIndices);

	TestVertex* vertexBuffer = (TestVertex*)(mappedGeometryPool + vertexBufferOffset);
	uint16* indexBuffer = (uint16*)(mappedGeometryPool + indexBufferOffset);

	memoryCopy(vertexBuffer, CubeVertices, sizeof(TestVertex) * countOf(CubeVertices));
	memoryCopy(indexBuffer, CubeIndices, sizeof(uint16) * countOf(CubeIndices));

	Entry& entry = entries[entryIndex];
	entry.vertexBufferOffset = vertexBufferOffset;
	entry.indexBufferOffset = indexBufferOffset;
	entry.vertexCount = countOf(CubeVertices);
	entry.indexCount = countOf(CubeIndices);
	entry.vertexStride = sizeof(TestVertex);

	return GeometryHandle(entryIndex);
}
