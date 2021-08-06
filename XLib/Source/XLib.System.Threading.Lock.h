#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.System.Threading.Atomics.h"

namespace XLib
{
	class ScopedLock;

	class Lock
	{
	private:
		Atomic<uint32> value;

	public:
		inline Lock() : value(0) {}

		inline void lock() { while (!value.compareExchange(1, 0)) {} }
		inline void unlock() { value.store(0); }
	};

	class ScopedLock : public NonCopyable
	{
	private:
		Lock &lock;

	public:
		inline ScopedLock(Lock& _lock) : lock(_lock) { lock.lock(); }
		inline ~ScopedLock() { lock.unlock(); }
	};
}