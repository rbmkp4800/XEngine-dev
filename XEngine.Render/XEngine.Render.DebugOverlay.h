#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.Vectors.h>
#include <XEngine.Gfx.HAL.D3D12.h>

namespace XEngine::Render
{
	enum class DebugOverlayOutputHandle : uint32 {};

	struct DebugOverlayUID
	{
		uint64 value;

		static constexpr inline DebugOverlayUID FromName(uint64 nameXSH, sint16 zIndex = 0);
	};

	class DebugOverlayDrawer : public XLib::NonCopyable
	{
	private:

	private:
		DebugOverlayDrawer();

	public:
		~DebugOverlayDrawer();

		void drawText(const char* text, float32x2 position, float32 scale, uint32 color32, uint32 backgroundColor = 0);
		void drawLine(float32x2 a, float32x2 b, uint32 color32);
		void drawRectFilled(float32x2 topLeft, float32x2 bottomRight, uint32 color32);
		void drawRectOutline(float32x2 topLeft, float32x2 bottomRight, uint32 color32);

		void flushBatchedDraws();
		void endDraw();

		uint16x2 getOutputSize() const;
	};

	class DebugOverlayHost abstract final
	{
		friend DebugOverlayDrawer;

	private:
		static constexpr uint16 MaxOutputCount = 16;
		static constexpr uint16 MaxOutputContentsTreeNodeCount = 1024;
		static constexpr uint16 PageSize = 8192;

	private:
		enum class BatchType : uint8
		{
			Undefied = 0,
			ColorTriangles,
			ColorLines,
			Text,
		};

		struct Batch
		{
			uint16 pageIndex;
			uint16 dataOffsetInPage;
			uint16 primitiveCount12_batchType4;
			uint16 batchesChainNextIdx;
		};

		struct OutputContentsTreeNode
		{
			uint64 overlayUID;

			uint16 batchesChainHeadIdx;
			uint16 pagesChainHeadIdx;
			uint16 pagesChainTailIdx;

			uint16 contentsTreeChildrenIdx[2];
			uint16 contentsTreeParentIdx;
			sint8 contentsTreeBalanceFactor;

			bool drawInProgress;
		};

		struct Page
		{
			uint16 pagesChainNextIdx;
		};

		struct Output
		{
			uint16 contentsTreeRootIdx;
		};

	private:
		static inline Gfx::HAL::Device* gfxHwDevice = nullptr;
		static inline Gfx::HAL::BufferHandle gfxHwPagesBuffer = {};

		static inline uint16 pageCount = 0;
		static inline uint16 freePagesChainHeadIdx = 0;

	public:
		static void CreateOutput(Gfx::HAL::Device& gfxHwDevice, uint8 outputId, uint16x2 outputSize);
		static void DestroyOutput(DebugOverlayOutputHandle outputHandle);

		static void SetOutputSize(DebugOverlayOutputHandle outputHandle, uint16x2 outputSize);

		static DebugOverlayUID CreateNewOverlayUID(sint16 zIndex);

		static DebugOverlayDrawer DrawOverlay(DebugOverlayUID overlayUID, uint8 outputId = 0);
		static void ClearOverlay(DebugOverlayUID overlayUID);
		static void ClearOverlayOutput(DebugOverlayUID overlayUID, uint8 outputId = 0);

#if 0
		static DebugOverlayHandle CreateOverlay();
		static void DestroyOverlay(DebugOverlayHandle overlayHandle);

		static DebugOverlayDrawer DrawOverlay(uint64 overlayNameXSH, uint8 outputId = 0);
		static DebugOverlayDrawer DrawOverlay(DebugOverlayHandle overlayHandle, uint8 outputId = 0);

		static void SetOverlayZIndex(uint64 overlayNameXSH, sint32 zIndex);
		static void SetOverlayZIndex(DebugOverlayDrawer overlayHandle, sint32 zIndex);
#endif
	};
}
