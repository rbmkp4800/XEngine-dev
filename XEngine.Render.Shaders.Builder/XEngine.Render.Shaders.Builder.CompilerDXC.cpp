#include <Windows.h>
#include <dxcapi.h>

#include <XLib.Containers.ArrayList.h>

#include "XEngine.Render.Shaders.Builder.CompilerDXC.h"

using namespace XLib;
using namespace XLib::Platform;
using namespace XEngine::Render::Shaders::Builder;

namespace
{
	struct DXCIncludeHandler : public IDxcIncludeHandler
	{
		virtual HRESULT __stdcall LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override;

		virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override { return E_NOINTERFACE; }
		virtual ULONG __stdcall AddRef() override { return 1; }
		virtual ULONG __stdcall Release() override { return 1; }
	};
}

HRESULT __stdcall ::DXCIncludeHandler::LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource)
{
	return S_FALSE;
}

CompilerDXC::CompilerDXC()
{
	DxcCreateInstance(CLSID_DxcCompiler, dxcCompiler.uuid(), dxcCompiler.voidInitRef());
}

CompilerResult CompilerDXC::compile(ShaderType type, SourcesCacheEntryId sourceId, SourcesCache& sourcesCache)
{
	SourcesCacheEntry& sourceCacheEntry = sourcesCache.getEntry(sourceId);
	const SourceText sourceText = sourceCacheEntry.loadText();

	DxcBuffer dxcSourceBuffer = {};
	dxcSourceBuffer.Ptr = sourceText.ptr;
	dxcSourceBuffer.Size = sourceText.size;
	dxcSourceBuffer.Encoding = CP_ACP; // CP_UTF8

	using DXCArgsList = ExpandableInplaceArrayList<LPCWSTR, 32, uint16, false>;
	DXCArgsList dxcArgsList;

	// Build arguments list
	{
		dxcArgsList.pushBack(L"-enable-16bit-types");

		LPCWSTR dxcProfile = nullptr;
		switch (type)
		{
			case ShaderType::CS: dxcProfile = L"-Tcs_6_6"; break;
			case ShaderType::VS: dxcProfile = L"-Tvs_6_6"; break;
			case ShaderType::MS: dxcProfile = L"-Tms_6_6"; break;
			case ShaderType::AS: dxcProfile = L"-Tas_6_6"; break;
			case ShaderType::PS: dxcProfile = L"-Tps_6_6"; break;
		}
		// ASSERT(dxcTargetProfile);
		dxcArgsList.pushBack(dxcProfile);
	}

	DXCIncludeHandler dxcIncludeHandler;

	COMPtr<IDxcResult> dxcResult;

	dxcCompiler->Compile(&dxcSourceBuffer, dxcArgsList.getData(), dxcArgsList.getSize(),
		&dxcIncludeHandler, dxcResult.uuid(), dxcResult.voidInitRef());

	CompilerResult result;
	return result;
}
