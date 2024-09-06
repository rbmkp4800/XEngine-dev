#include <XLib.Math.Matrix4x4.h>

#include "XEngine.Render.Scene.h"

using namespace XEngine::Gfx;
using namespace XEngine::Render;

void Scene::initialize(HAL::Device& gfxHwDevice)
{
	XEAssert(!this->gfxHwDevice);
	this->gfxHwDevice = &gfxHwDevice;

	gfxHwTransformsBuffer = gfxHwDevice.createStagingBuffer(64 * 1024, Gfx::HAL::StagingBufferAccessMode::DeviceReadHostWrite);
	mappedTransformsBuffer = (XLib::Matrix4x4*)gfxHwDevice.getMappedBufferPtr(gfxHwTransformsBuffer);
}

TransformSetHandle Scene::allocateTransformSet(uint16 tramsformSetSize)
{
	const uint16 baseTransformIndex = allocatedTansformCount;
	allocatedTansformCount += tramsformSetSize;
	return TransformSetHandle(baseTransformIndex);
}

GeometryInstanceHandle Scene::createGeometryInstance(GeometryHandle geometryHandle,
	TransformSetHandle transformSetHandle, uint16 baseTransformOffset)
{
	XEAssert(geometryInstanceCount < countOf(geometryInstances));
	const uint32 geometryInstanceIndex = geometryInstanceCount;
	geometryInstanceCount++;

	GeometryInstance& geometryInstance = geometryInstances[geometryInstanceIndex];
	geometryInstance.geometryHandle = geometryHandle;
	geometryInstance.baseTransformIndex = uint16(transformSetHandle) + baseTransformOffset;

	return GeometryInstanceHandle(geometryInstanceIndex);
}

void Scene::updateTransform(TransformSetHandle handle, uint16 transformIndex, const XLib::Matrix4x4& transform)
{
	mappedTransformsBuffer[uint16(handle) + transformIndex] = transform;
}
