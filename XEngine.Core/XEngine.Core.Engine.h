#pragma once

#include <XLib.h>
#include <XLib.Vectors.h>

namespace XEngine::Core { class DiskWorker; }
namespace XEngine::Render { class Device; }
namespace XEngine::Render { class Target; }

namespace XEngine::Core { class Engine; }

namespace XEngine::Core
{
	class GameBase abstract
	{
		friend Engine;

	protected:
		virtual void initialize() = 0;
		virtual void update(float32 timeDelta) = 0;
	};

	class Engine abstract final
	{
	public:
		static DiskWorker& GetDiskWorker();
		static Render::Device& GetRenderDevice();

		static uint32 GetOutputViewCount();
		static Render::Target& GetOutputViewRenderTarget(uint32 outputViewIndex);
		static uint16x2 GetOutputViewRenderTargetSize(uint32 outputViewIndex);

		static void Run(GameBase* game);
		static void Shutdown();
	};
}