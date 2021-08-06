#include <Unknwn.h>

#include "XLib.Platform.COMPtr.h"

void XLib::Platform::Internal::IUnknown_AddRef(void* ptr)
{
	((IUnknown*)ptr)->AddRef();
}

void XLib::Platform::Internal::IUnknown_Release(void* ptr)
{
	((IUnknown*)ptr)->Release();
}
