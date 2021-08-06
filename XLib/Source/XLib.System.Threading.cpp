#include <Windows.h>

#include "XLib.System.Threading.h"

#include "XLib.Debug.h"

using namespace XLib;

struct ThreadMainWrapperArgs
{
	RawDelegate rawDelegate;
	volatile bool ready;
};

static DWORD __stdcall ThreadMainWrapper(void* _args)
{
	ThreadMainWrapperArgs *args = (ThreadMainWrapperArgs*)_args;
	Delegate<void> threadMain(args->rawDelegate);
	args->ready = true;
	threadMain.call();

	return 0;
}

void ISystemHandle::destroy()
{
	if (handle)
	{
		CloseHandle(handle);
		handle = nullptr;
	}
}

void Thread::_create(ThreadMainProc<void> threadMainProc, void* args, bool suspended)
{
	DWORD threadId = 0;
	handle = CreateThread(nullptr, 0, threadMainProc, args, suspended ? CREATE_SUSPENDED : 0, &threadId);
}
void Thread::create(Delegate<void> threadMainDelegate, bool suspended)
{
	DWORD threadId = 0;
	ThreadMainWrapperArgs args;
	args.rawDelegate = threadMainDelegate.toRaw();
	args.ready = false;
	handle = CreateThread(nullptr, 0, ThreadMainWrapper, (void*)&args, suspended ? CREATE_SUSPENDED : 0, &threadId);

	while (!args.ready)
		{ }
}
void Thread::terminate(uint32 exitCode) { TerminateThread(handle, exitCode); }
void Thread::suspend() { SuspendThread(handle); }
void Thread::resume() { ResumeThread(handle); }

void Thread::Sleep(uint32 milliseconds) { ::Sleep(milliseconds); }
void Thread::YieldExecution() { ::SwitchToThread(); }

bool WaitableBase::wait()
{
	DWORD result = WaitForSingleObject(handle, INFINITE);
	//if (result == WAIT_FAILED)
	//	Debug::LogLastSystemError(SysErrorDbgMsgFmt);
	return result == WAIT_OBJECT_0;
}
bool WaitableBase::wait(uint32 timeout)
{
	DWORD result = WaitForSingleObject(handle, timeout);
	//if (result == WAIT_FAILED)
	//	Debug::LogLastSystemError(SysErrorDbgMsgFmt);
	return result == WAIT_OBJECT_0;
}

bool _private::WaitAll(void** handles, uint32 handleCount)
{
	DWORD result = WaitForMultipleObjects(handleCount, handles, true, INFINITE);
	if (result == WAIT_FAILED)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	return true;
}