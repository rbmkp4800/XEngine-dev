#include <Windows.h>

#include "XLib.System.Threading.Event.h"

using namespace XLib;

void Event::initialize(bool state, bool manualReset)
{
	destroy();
	handle = CreateEvent(nullptr, manualReset, state, nullptr);
}
void Event::set()
{
	XAssert(isInitialized());
	SetEvent(handle);
}
void Event::reset()
{
	XAssert(isInitialized());
	ResetEvent(handle);
}
void Event::pulse()
{
	XAssert(isInitialized());
	PulseEvent(handle);
}
