#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.Platform.COMPtr.h>

#include "XEngine.Render.Base.h"

struct ID3D12Resource;
struct ID3D12GraphicsCommandList;
struct ID3D12CommandSignature;
struct ID3D12PipelineState;

namespace XLib { struct Matrix3x4; }
namespace XEngine::Render { class Device; }
namespace XEngine::Render::Device_ { class Renderer; }

namespace XEngine::Render
{
	class Scene : public XLib::NonCopyable
	{
		friend Device_::Renderer;

	private:
		struct InstanceHeader
		{
			uint16 magic;
			uint8 generation;
			uint8 cachedBundleRootNodeCount;

			GeometrySectionBundleHandle bundle;

			TransformAddress baseTransform;
		};

		struct DrawBatchDesc
		{
			uint16 shader;
			uint16 sectionCount;
			uint32 sectionsRecordsBaseOffsetX256;
		};

		using DrawBatchesList = XLib::ArrayList<DrawBatchDesc, uint16, false>;

	private:
		Device* device = nullptr;

		//byte* instancesArena = nullptr;
		//byte* cpuBvhArena = nullptr;

		DrawBatchesList drawBatches;

		XLib::Platform::COMPtr<ID3D12Resource> d3dTransformsBuffer;
		XLib::Platform::COMPtr<ID3D12Resource> d3dDrawArgsBuffer;

		XLib::Matrix3x4* mappedTransformsBuffer = nullptr;
		byte* mappedDrawArgsBuffer = nullptr;

		uint32 allocatedTansformCount = 0;

	public:
		Scene() = default;
		~Scene() = default;

		void initialize(Device& device);
		void destroy();

		TransformAddress allocateTransforms(uint32 size = 1);
		void releaseTransforms(TransformAddress address);

		//InplaceGeometrySectionsInstanceHandle insertInplaceGeometrySections(SectionDesc* sectionDescs, uint32 sectionCount,);
		//void removeInplaceGeometrySections(InplaceGeometrySectionsInstanceHandle handle);

		GeometrySectionBundleInstanceHandle insertGeometrySectionBundleInstance(
			GeometrySectionBundleHandle bundleHandle, TransformAddress baseTransform);
		void removeGeomertySectionBundleInstance(GeometrySectionBundleInstanceHandle handle);

		//LightHandle insertLight();
		//void removeLight(LightHandle handle);

		void updateTransforms(TransformAddress startAddress, uint32 count, const XLib::Matrix3x4* transforms);

		//void enableGeometrySectionBundleInstanceMaterialSetOverride(GeometrySectionBundleInstanceHandle instance, bool enable);
		//void updateGeometrySectionBundleInstanceMaterialSetOverride(GeometrySectionBundleInstanceHandle instance, ...);

		inline void updateTransform(TransformAddress address, const XLib::Matrix3x4& transform) { updateTransforms(address, 1, &transform); }
	};
}
