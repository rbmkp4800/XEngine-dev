#include <Windows.h>

#include "XLib.System.Threading.Event.h"
#include "XLib.Debug.h"

using namespace XLib;

void Event::initialize(bool state, bool manualReset)
{
	destroy();
	handle = CreateEvent(nullptr, manualReset, state, nullptr);
}
void Event::set()
{
	XASSERT(isInitialized(), "not initialized");
	SetEvent(handle);
}
void Event::reset()
{
	XASSERT(isInitialized(), "not initialized");
	ResetEvent(handle);
}
void Event::pulse()
{
	XASSERT(isInitialized(), "not initialized");
	PulseEvent(handle);
}