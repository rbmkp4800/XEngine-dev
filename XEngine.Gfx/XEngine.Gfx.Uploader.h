#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

namespace XEngine::Gfx
{
	class UploadContext
	{

	};

	class Uploader final : public XLib::NonCopyable
	{
	private:

	public:
		Uploader() = default;
		~Uploader() = default;

		void initialize();
		void destroy();

		void uploadBuffer(HAL::BufferHandle destBuffer,
			uint32 destOffset, const void* sourceData, uint32 size,
			UploadContext& context, UploadStatusUpdateCallback statusUpdateCallback);

		void uploadTexture(HAL::TextureHandle destTexture, HAL::TextureSubresource destSubresource,
			HAL::TextureRegion destRegion, const void* sourceData, uint32 sourceRowPitch,
			UploadContext& context, UploadStatusUpdateCallback statusUpdateCallback);

		// uploadBufferFromMemory
		// uploadBufferFromDisk
		// uploadTextureFromMemory
		// uploadTextureFromDisk

		CancelUploadResult cancelUpload(UploadContext& context);
	};
}
