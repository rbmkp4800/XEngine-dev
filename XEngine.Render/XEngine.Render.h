#pragma once

#include <XEngine.Render.HAL.D3D12.h>

#include "XEngine.Render.ShaderLibrary.h"

namespace XEngine::Render
{
	class Host abstract final
	{
	private:
		static HAL::Device halDevice;
		static ShaderLibrary shaderLibrary;

	public:
		static void Initialize();

		static inline HAL::Device& GetDevice() { return halDevice; }
		static inline const ShaderLibrary& GetShaders() { return shaderLibrary; }
	};
}
