#include "XEngine.ThreadPool.h"

#include <XLib.System.Threading.h>

using namespace XEngine;
using namespace XLib;

#if 0

namespace
{
	static constexpr uint32 MaxThreadCount = 64;

	struct ThreadRecord
	{
		XLib::Thread thread;
	};

	bool intitialized = false;
	ThreadRecord threads[MaxThreadCount];
}

void ThreadPool::WorkerThreadMain(const uint32 threadIndex)
{
	for (;;)
	{
		Task* task = ;//queue.dequeue(task);
		task->routine(*task);
	}
}

void ThreadPool::QueueTaskInternal(Task& task, TaskRoutine<Task> routine)
{
	// ASSERT(initialized);

	//tasksQueue.enqueue(task);
}

void ThreadPool::Run(uint32 initialThreadCount, ThreadOccupantRoutine mainRoutine, void* mainRoutineArg)
{
	// ASSERT(!intitialized); // This should be done thread safe using CAS
	// ASSERT(threadCount > maxThreadCount);

	//tasksQueue.initialize();



	if (userManagedThreadCount < totalThreadCount)
	{
		const uint32 internalThreadCount = totalThreadCount - userManagedThreadCount;
		for (uint32 i = 0; i < internalThreadCount; i++)
		{
			internalThreads[i].create(InternalThreadMain, i);
		}
	}
	else
	{
		// TODO: Warning
	}

	intitialized = true;
}

void ThreadPool::Shutdown()
{
	// TODO: Wait for all threads and destroy them

	intitialized = false;
}

#endif