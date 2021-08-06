#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.System.Threading.Atomics.h"

namespace XLib
{
	class ScopedReaderLock;
	class ScopedWriterLock;

	class ReadersWriterLock : public NonCopyable
	{
		friend ScopedReaderLock;
		friend ScopedWriterLock;

	private:
		static constexpr uint32 writerLockMask = 1 << 31;

		Atomic<uint32> lock;

		inline void readerLock()
		{
			if (lock.increment() >= writerLockMask)
				while (lock.load() & writerLockMask) {}
		}
		inline void writerLock()
		{
			while (!lock.compareExchange(writerLockMask, 0)) {}
		}
		inline void readerUnlock()
		{
			lock.decrement();
		}
		inline void writerUnlock()
		{
			lock.sub(writerLockMask);
		}

	public:
		inline ReadersWriterLock() : lock(0) {}
	};

	class ScopedReaderLock : public NonCopyable
	{
	private:
		ReadersWriterLock &lock;

	public:
		inline ScopedReaderLock(ReadersWriterLock& _lock) : lock(_lock) { lock.readerLock(); }
		inline ~ScopedReaderLock() { lock.readerUnlock(); }
	};

	class ScopedWriterLock : public NonCopyable
	{
	private:
		ReadersWriterLock &lock;

	public:
		inline ScopedWriterLock(ReadersWriterLock& _lock) : lock(_lock) { lock.writerLock(); }
		inline ~ScopedWriterLock() { lock.writerUnlock(); }
	};
}