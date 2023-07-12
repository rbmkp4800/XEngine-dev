#pragma once

#include <XEngine.GfxHAL.D3D12.h>

#include "XEngine.Render.ShaderLibrary.h"

namespace XEngine::Render
{
	class Host abstract final
	{
	private:
		static GfxHAL::Device halDevice;
		static ShaderLibrary shaderLibrary;

	public:
		static void Initialize();

		static inline GfxHAL::Device& GetDevice() { return halDevice; }
		static inline const ShaderLibrary& GetShaders() { return shaderLibrary; }
	};
}
