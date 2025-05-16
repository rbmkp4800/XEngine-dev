#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

#include "XEngine.Render.GeometryHeap.h"

namespace XLib { struct Matrix4x4; }
namespace XEngine::Render { class SceneRenderer; }

namespace XEngine::Render
{
	enum class TransformSetHandle : uint32 {};
	enum class GeometryInstanceHandle : uint32 {};

	class Scene : public XLib::NonCopyable
	{
		friend SceneRenderer;

	private:
		struct GeometryInstance
		{
			GeometryHandle geometryHandle;
			uint16 baseTransformIndex;
		};

	private:
		Gfx::HAL::Device* gfxHwDevice = nullptr;

		Gfx::HAL::BufferHandle gfxHwTransformsBuffer = {};
		XLib::Matrix4x4* mappedTransformsBuffer = nullptr;
		uint16 allocatedTansformCount = 0;

		GeometryInstance geometryInstances[16] = {};
		uint16 geometryInstanceCount = 0;

	public:
		Scene() = default;
		~Scene() = default;

		void initialize(Gfx::HAL::Device& gfxHwDevice);
		void destroy();

		TransformSetHandle allocateTransformSet(uint16 tramsformSetSize = 1);
		void releaseTransformSet(TransformSetHandle transformSetHandle);

		GeometryInstanceHandle createGeometryInstance(GeometryHandle geometryHandle,
			TransformSetHandle transformSetHandle, uint16 baseTransformOffset = 0);
		void destroyGeometryInstance(GeometryInstanceHandle geometryInstanceHandle);

		void updateTransform(TransformSetHandle handle, uint16 transformIndex, const XLib::Matrix4x4& transform);
	};
}
