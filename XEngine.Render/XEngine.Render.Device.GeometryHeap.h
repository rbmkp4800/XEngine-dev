#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include "XEngine.Render.Base.h"

namespace XEngine::Render { class Device; }

namespace XEngine::Render::Device_
{
	class GeometryHeap : public XLib::NonCopyable
	{
	public:
		struct SectionBundleDesc
		{
			GeometrySectionDesc* sections;
			uint16 sectionCount;
		};

	private:
		Device& device;

		SectionBundleDesc sectionBundles[16];
		uint16 sectionBundleCount = 0;

	public:
		GeometryHeap(Device& device) : device(device) {}
		~GeometryHeap() = default;

		void initialize();
		void destroy();

		GeometrySectionBundleHandle createSectionBundle(const GeometrySectionDesc* sections, uint32 sectionCount);
		void destroySectionBundle(GeometrySectionBundleHandle handle);

		inline const SectionBundleDesc& getSectionBundle(GeometrySectionBundleHandle handle) const { return sectionBundles[handle]; }
	};
}
