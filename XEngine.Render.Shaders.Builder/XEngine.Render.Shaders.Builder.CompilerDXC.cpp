#include <Windows.h>
#include <wrl.h>
#include <dxcapi.h>

#include "XEngine.Render.Shaders.Builder.CompilerDXC.h"

using namespace Microsoft::WRL;
using namespace XEngine::Render::Shaders::Builder;

CompilerResult CompilerDXC::compile(ShaderType type, const char* filename, SourcesCache& sourcesCache)
{
	HMODULE hDXCompiler = LoadLibraryA("dxcompiler.dll");
	DxcCreateInstanceProc dxcCreateInstance = DxcCreateInstanceProc(GetProcAddress(hDXCompiler, "DxcCreateInstance"));

	ComPtr<IDxcUtils> dxcUtils;
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils.GetAddressOf()));

	ComPtr<IDxcUtils> dxcCompiler;
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(dxcUtils.GetAddressOf()));

	CompilerResult result;
	return result;
}
