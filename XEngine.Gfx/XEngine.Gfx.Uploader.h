#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

namespace XEngine::Gfx
{
#if 0
	class UploadContext
	{

	};
#endif

	class Uploader final : public XLib::NonCopyable
	{
	private:
		HAL::Device* hwDevice = nullptr;

		HAL::CommandAllocatorHandle hwCommandAllocator = {};
		HAL::BufferHandle hwUploadBuffer = {};

		void* mappedUploadBuffer = nullptr;

	public:
		Uploader() = default;
		~Uploader() = default;

		void initialize(HAL::Device& hwDevice);
		void destroy();

		void uploadBuffer(HAL::BufferHandle hwDestBuffer,
			uint32 destOffset, const void* sourceData, uint32 size);

		void uploadTexture(HAL::TextureHandle hwDestTexture, HAL::TextureSubresource hwDestSubresource,
			HAL::TextureRegion hwDestRegion, const void* sourceData, uint32 sourceRowPitch);

#if 0
		void uploadBuffer(HAL::BufferHandle hwDestBuffer,
			uint32 destOffset, const void* sourceData, uint32 size,
			UploadContext& context, UploadStatusUpdateCallback statusUpdateCallback);

		void uploadTexture(HAL::TextureHandle hwDestTexture, HAL::TextureSubresource hwDestSubresource,
			HAL::TextureRegion hwDestRegion, const void* sourceData, uint32 sourceRowPitch,
			UploadContext& context, UploadStatusUpdateCallback statusUpdateCallback);

		CancelUploadResult cancelUpload(UploadContext& context);
#endif
	};

	extern Uploader GlobalUploader;
}
