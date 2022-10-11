#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

#include "XEngine.ThreadPool.h"

namespace XEngine { class AsyncMemoryCopyContext; }

namespace XEngine
{
	using AsyncMemoryCopyCompletionCallback = void(*)(AsyncMemoryCopyContext&);

	void AsyncMemoryCopy(void* destination, const void* source, uintptr size,
		AsyncMemoryCopyCompletionCallback completionCallback, AsyncMemoryCopyContext& context);

	class AsyncMemoryCopyContext
	{
		friend void XEngine::AsyncMemoryCopy(void* destination, const void* source, uintptr size,
			AsyncMemoryCopyCompletionCallback completionCallback, AsyncMemoryCopyContext& context);

	private:
		struct Task : public ThreadPool::Task {};

	private:
		Task task;
		void* destination = nullptr;
		const void* source = nullptr;
		uintptr size = 0;
		AsyncMemoryCopyCompletionCallback completionCallback = nullptr;

	private:
		static void TaskRoutine(Task& task);

	public:
		AsyncMemoryCopyContext() = default;
		~AsyncMemoryCopyContext() = default;
	};
}

// AsyncMemoryCopy is guaranteed to be asynchronous. All "small synchronous copy" optimizations should
// be done on user side. Callback is guaranteed to be called on ThreadPool.
