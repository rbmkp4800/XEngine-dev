#include "XEngine.ThreadPoolUtils.h"

#include "XEngine.ThreadPool.h"

using namespace XLib;
using namespace XEngine;

static constexpr uintptr asyncMemoryCopyBytesPerTaskRun = 0x40000;

void XEngine::AsyncMemoryCopy(void* destination, const void* source, uintptr size,
	AsyncMemoryCopyCompletionCallback completionCallback, AsyncMemoryCopyContext& context)
{
	context.destination = destination;
	context.source = source;
	context.size = size;
	context.completionCallback = completionCallback;

	ThreadPool::QueueTask(context.task, &AsyncMemoryCopyContext::TaskRoutine);
}

void AsyncMemoryCopyContext::TaskRoutine(AsyncMemoryCopyContext::Task& task)
{
	AsyncMemoryCopyContext& context =
		*(AsyncMemoryCopyContext*)(uintptr(&task) - offsetof(AsyncMemoryCopyContext, task));

	// ASSERT(context.destination);
	// ASSERT(context.source);
	// ASSERT(context.size);
	// ASSERT(context.completionCallback);

	const bool isFinalPiece = context.size <= asyncMemoryCopyBytesPerTaskRun;
	const uintptr pieceSize = isFinalPiece ? context.size : asyncMemoryCopyBytesPerTaskRun;

	memoryCopy(context.destination, context.source, pieceSize);

	if (isFinalPiece)
	{
		const AsyncMemoryCopyCompletionCallback localCompletionCallback = context.completionCallback;

		context.destination = nullptr;
		context.source = nullptr;
		context.size = 0;
		context.completionCallback = nullptr;

		localCompletionCallback(context);
	}
	else
	{
		(uintptr&)context.destination += pieceSize;
		(uintptr&)context.source += pieceSize;
		context.size -= pieceSize;

		ThreadPool::QueueTask(context.task, &AsyncMemoryCopyContext::TaskRoutine);
	}
}
