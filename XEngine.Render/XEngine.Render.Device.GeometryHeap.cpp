#include <XLib.SystemHeapAllocator.h>

#include "XEngine.Render.Device.h"

using namespace XLib;
using namespace XEngine::Render;
using namespace XEngine::Render::Device_;

void GeometryHeap::initialize()
{
	sectionBundleCount = 0;
}

GeometrySectionBundleHandle GeometryHeap::createSectionBundle(
	const GeometrySectionDesc* sections, uint32 sectionCount)
{
	const uint16 bundleIndex = sectionBundleCount;
	sectionBundleCount++;

	SectionBundleDesc& bundle = sectionBundles[bundleIndex];

	bundle.sections = (GeometrySectionDesc*)
		SystemHeapAllocator::Allocate(sizeof(GeometrySectionDesc) * sectionCount);
	memoryCopy(bundle.sections, sections, sizeof(GeometrySectionDesc) * sectionCount);

	bundle.sectionCount = uint16(sectionCount);

	return GeometrySectionBundleHandle(bundleIndex);
}
