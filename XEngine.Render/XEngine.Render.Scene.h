#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XEngine.Gfx.HAL.D3D12.h>

namespace XEngine::Render
{
	class Scene : XLib::NonCopyable
	{
	//private:
	public:
		Gfx::HAL::Device* gfxHwDevice = nullptr;

		Gfx::HAL::BufferHandle gfxHwTestModel = {};
		void* mappedTestModel = nullptr;

	public:
		Scene() = default;
		~Scene() = default;

		void initialize(Gfx::HAL::Device& gfxHwDevice);
		void destroy();
	};
}
