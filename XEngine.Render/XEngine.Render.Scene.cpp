#include <d3d12.h>

#include <XLib.Math.Matrix3x4.h>
#include <XLib.Math.Matrix4x4.h>
#include <XLib.Memory.h>
#include <XLib.Platform.D3D12.Helpers.h>

#include "XEngine.Render.Scene.h"

#include "XEngine.Render.Device.h"

using namespace XLib;
using namespace XEngine::Render;

void Scene::initialize(Device& device)
{
	this->device = &device;

	ID3D12Device *d3dDevice = device.d3dDevice;

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Buffer(0x1'0000),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		d3dTransformsBuffer.uuid(), d3dTransformsBuffer.voidInitRef());

	d3dDevice->CreateCommittedResource(
		&D3D12HeapProperties(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
		&D3D12ResourceDesc_Buffer(0x1'0000),
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		d3dDrawArgsBuffer.uuid(), d3dDrawArgsBuffer.voidInitRef());

	d3dTransformsBuffer->Map(0, &D3D12Range(), to<void**>(&mappedTransformsBuffer));
	d3dDrawArgsBuffer->Map(0, &D3D12Range(), to<void**>(&mappedDrawArgsBuffer));
}

TransformAddress Scene::allocateTransforms(uint32 size)
{
	const uint32 baseTransformIndex = allocatedTansformCount;
	allocatedTansformCount += size;

	return TransformAddress(baseTransformIndex);
}

GeometrySectionBundleInstanceHandle Scene::insertGeometrySectionBundleInstance(
	GeometrySectionBundleHandle bundleHandle, TransformAddress baseTransform)
{
	if (drawBatches.isEmpty())
	{
		DrawBatchDesc& batch = drawBatches.emplaceBack();
		batch.shader = 0;
		batch.sectionCount = 0;
		batch.sectionsRecordsBaseOffsetX256 = 0;
	}

	DrawBatchDesc& batch = drawBatches.back();

	const Device_::GeometryHeap::SectionBundleDesc& bundle = device->geometryHeap.getSectionBundle(bundleHandle);

	struct SectionRecord
	{
		uint64 a;
		uint64 b;
	};

	SectionRecord sectionRecords[64];

	for (uint16 i = 0; i < bundle.sectionCount; i++)
	{
		uint64 a = 0;

		a |= bundle.sections[i].clustersAddress / 256;
		a |= uint64(baseTransform) << 24;
		// a |= ... *material* ;

		const uint8 clusterCount = bundle.sections[i].clusterCount;
		uint64 b = clusterCount == 64 ? uint64(-1) : (uint64(1) << clusterCount) - 1;

		sectionRecords[i] = { a, b };
	}

	Memory::Copy(mappedDrawArgsBuffer + sizeof(SectionRecord) * batch.sectionCount,
		&sectionRecords, sizeof(SectionRecord) * bundle.sectionCount);

	batch.sectionCount += bundle.sectionCount;

	return 0;
}

void Scene::updateTransforms(TransformAddress startAddress, uint32 count, const XLib::Matrix3x4* transforms)
{
	Memory::Copy(mappedTransformsBuffer + uintptr(startAddress), transforms, sizeof(Matrix3x4) * count);
}
