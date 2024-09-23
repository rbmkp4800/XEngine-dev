#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

#include "XEngine.Render.DebugOverlay.h"

namespace XEngine::Render
{
	class DebugOverlayRenderer : public XLib::NonCopyable
	{
	private:
		Gfx::HAL::PipelineLayoutHandle gfxHwPipelineLayout = {};

		Gfx::HAL::GraphicsPipelineHandle gfxHwColorTrianglesPipeline = {};
		Gfx::HAL::GraphicsPipelineHandle gfxHwColorLinesPipeline = {};
		Gfx::HAL::GraphicsPipelineHandle gfxHwTextPipeline = {};

	public:
		DebugOverlayRenderer() = default;
		~DebugOverlayRenderer() = default;

		void initialize(Gfx::HAL::Device& gfxHwDevice);

		void render(Gfx::HAL::TextureHandle gfxHwTargetTexture, DebugOverlayOutputHandle outputHandle);
	};
}
