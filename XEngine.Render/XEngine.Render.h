#pragma once

#include <XEngine.Render.HAL.D3D12.h>

namespace XEngine::Render
{
	class Host abstract final
	{
	private:
		static HAL::Device halDevice;

	public:
		static void Initialize();

		static inline HAL::Device& GetDevice() { return halDevice; }
	};
}
