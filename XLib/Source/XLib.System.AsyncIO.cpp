#include <Windows.h>

#include "XLib.h"
#include "XLib.System.AsyncIO.h"
#include "XLib.System.Network.Socket.h"
#include "XLib.System.NamedPipe.h"

using namespace XLib;

constexpr ULONG_PTR xlibCompletitionKey = ULONG_PTR(0xFFFFFFFF'FFFFFFFE);
constexpr ULONG_PTR xlibTerminateCompletitionKey = ULONG_PTR(0xFFFFFFFF'FFFFFFFF);

void AsyncIODispatcher::initialize()
{
	/*static_assert(offsetof(DispatchedAsyncTask, overlapped) == 0,
		"XLib.System.AsyncIO.Internal.DispatchedAsyncTask.overlapped offset must be 0");*/

	static_assert(sizeof(DispatchedAsyncTask::overlapped) == sizeof(OVERLAPPED),
		"XLib.System.AsyncIO.Internal.DispatchedAsyncTask.overlapped size != OVERLAPPED size");	

	destroy();
	hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
}
void AsyncIODispatcher::destroy()
{
	if (hIOCP)
	{
		CloseHandle(hIOCP);
		hIOCP = nullptr;
	}
}

void AsyncIODispatcher::associate(Socket& socket)
	{ CreateIoCompletionPort(socket.getHandle(), hIOCP, xlibCompletitionKey, 0); }
void AsyncIODispatcher::associate(NamedPipe& pipe)
	{ CreateIoCompletionPort(pipe.getHandle(), hIOCP, xlibCompletitionKey, 0); }

void AsyncIODispatcher::dispatchAll()
{
	for (;;)
	{
		DWORD transferredSize = 0;
		ULONG_PTR key = 0;
		DispatchedAsyncTask* task = nullptr;
		BOOL result = GetQueuedCompletionStatus(hIOCP, &transferredSize, &key, (OVERLAPPED**)&task, INFINITE);

		if (key == xlibTerminateCompletitionKey)
			return;
		if (!result && !task)
		{
			//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
			return;
		}

		if (key == xlibCompletitionKey)
		{
			switch (task->state)
			{
				case DispatchedAsyncTask::State::Transfer:
				{
					TransferCompletedHandler handler(task->rawHandler);
					uintptr userKey = task->key;
					task->clear();

					if (!transferredSize)
						result = 0;
					handler.call(result != FALSE, uint32(transferredSize), userKey);
					break;
				}

				case DispatchedAsyncTask::State::SocketAccept:
				{
					SocketAcceptedHandler handler(task->rawHandler);
					uintptr userKey = task->key;
					task->clear();

					TCPListenSocket::DispatchedAsyncTask *acceptTask = (TCPListenSocket::DispatchedAsyncTask*)task;
					handler.call(result != FALSE, acceptTask->acceptedSocket, acceptTask->extractIPAddress(), userKey);
					break;
				}

				case DispatchedAsyncTask::State::NamedPipeConnect:
				{
					NamedPipeConnectedHandler handler(task->rawHandler);
					uintptr userKey = task->key;
					task->clear();

					handler.call(result != FALSE, userKey);
					break;
				}

				default:
					// TODO: warning
					break;
			}
		}
		else
		{
			Delegate<void> handler(RawDelegate((void*)key, (void*)task));
			handler.call();
		}
	}
}
void AsyncIODispatcher::cleanupQueue()
{
	for (;;)
	{
		DWORD dwDummy = 0;
		ULONG_PTR ulptrDummy = 0;
		OVERLAPPED* overlapper = nullptr;
		BOOL result = GetQueuedCompletionStatus(hIOCP, &dwDummy, &ulptrDummy, &overlapper, 0);
		if (!result && !overlapper)
			return;
	}
}

void AsyncIODispatcher::invokeShutdown()
	{ PostQueuedCompletionStatus(hIOCP, 0, xlibTerminateCompletitionKey, nullptr); }

void AsyncIODispatcher::invoke(Delegate<void> handler)
{
	RawDelegate rawHandler = handler.toRaw();
	PostQueuedCompletionStatus(hIOCP, 0, ULONG_PTR(rawHandler.getObject()), (OVERLAPPED*)rawHandler.getMethod());
}