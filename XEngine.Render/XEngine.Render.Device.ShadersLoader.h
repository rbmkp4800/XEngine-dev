#pragma once

#include <XLib.h>

namespace XEngine::Render::Device_
{
	class ShadersLoader
	{
	private:

	public:
		using PSOToken = uint32;

		struct PSODesc
		{
			const char* vsName;
			const char* asName;
			const char* msName;
			const char* psName;
		};

	public:
		ShadersLoader() = default;
		~ShadersLoader() = default;

		PSOToken registerPSO(PSODesc& desc);
		void unregisterPSO(PSOToken token);

		void* acquirePSO(PSOToken token);
	};
}
