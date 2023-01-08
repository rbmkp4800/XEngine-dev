#pragma once

#include <XEngine.Render.HAL.D3D12.h>

#include "XEngine.Render.ShaderLibrary.h"

namespace XEngine::Render
{
	class Shaders abstract final
	{
	public:
		static const ShaderLibrary& GetLibrary();
	};

	class Host abstract final
	{
	private:
		static HAL::Device halDevice;

	public:
		static void Initialize();

		static inline HAL::Device& GetDevice() { return halDevice; }
	};
}
