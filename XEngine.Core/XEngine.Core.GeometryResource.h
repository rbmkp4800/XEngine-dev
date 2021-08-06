#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include <XEngine.Render.Device.h>

#include "XEngine.Core.DiskWorker.h"

namespace XEngine::Core
{
#if 0
	class GeometryResourceLoader;

	class GeometryResource : public XLib::NonCopyable
	{
		friend GeometryResourceLoader;

	private:
		uint32 vertexCount = 0;
		uint32 indexCount = 0;
		Render::BufferAddress buffer = Render::BufferAddress(0);
		uint8 vertexStride = 0;
		bool indexIs32Bit = false;

	public:
		GeometryResource() = default;
		~GeometryResource() = default;
	};

	class GeometryResourceLoader : public XLib::NonCopyable
	{
	public:
		struct DiskReadContext
		{
			DiskOperationContext diskReadContext;
			Render::Device::AsyncUploadContext renderUploadContext;
		};

		struct GeneratorContext
		{
			Render::Device_::Uploader::OperationHandle renderUploadOperationHandle;
		};

		using CompletionCallback = void(*)();

	private:
		CompletionCallback completionCallback;
		Render::Device& renderDevice;
		DiskWorker* diskWorker;

	public:
		GeometryResourceLoader(CompletionCallback completionCallback,
			Render::Device& renderDevice, DiskWorker* diskWorker = nullptr);
		~GeometryResourceLoader() = default;

		void diskReadFromVFS(uint32 id, float32 priority, DiskReadContext& asyncContext);
		void diskReadFromFile(const char* filename, float32 priority, DiskReadContext& asyncContext);
		void generatePlane(float32 priority, GeneratorContext& asyncContext);
		void generateCube(float32 priority, GeneratorContext& asyncContext);
		void generateCubicSphere(uint16 subdivisionCount, float32 priority, GeneratorContext& asyncContext);
		void generatePolarSphere(uint16 subdivisionCount, float32 priority, GeneratorContext& asyncContext);

		void setLoadingPriority(float32 priority, DiskReadContext& asyncContext);
		void setLoadingPriority(float32 priority, GeneratorContext& asyncContext);
		void cancelLoading(DiskReadContext& asyncContext);
		void cancelLoading(GeneratorContext& asyncContext);
	};
#endif
}
