#pragma once

#include "XLib.h"
#include "XLib.Debug.h"
#include "XLib.NonCopyable.h"
#include "XLib.System.Threading.Event.h"
#include "XLib.System.Threading.Atomics.h"

namespace XLib
{
	enum class BarrierBlockMode
	{
		CyclicLocking,
		CyclicLockFree,
	};

	enum class BarrierTriggerType
	{
		Internal,
		External,
	};

	template <BarrierBlockMode blockMode, BarrierTriggerType triggerType>
	class Barrier;

	template <>
	class Barrier<BarrierBlockMode::CyclicLocking, BarrierTriggerType::Internal> : public NonCopyable
	{
	private:
		Event events[2];
		Atomic<uint32> counter;
		uint32 threadCount;
		uint8 eventSelector;

	public:
		inline Barrier() : counter(0), threadCount(0), eventSelector(0) {}
		inline Barrier(uint32 _threadCount) { initialize(_threadCount); }
		inline ~Barrier()
		{
			counter.value = 0;
			threadCount = 0;
			eventSelector = 0;
		}

		inline void initialize(uint32 _threadCount)
		{
			XASSERT(_threadCount > 0, "invalid thread count value");

			counter.value = 0;
			events[0].initialize(false);
			events[1].initialize(false);
			threadCount = _threadCount;
			eventSelector = 0;
		}
		inline void destroy() { this->~Barrier(); }

		template <typename PreTriggerAction>
		inline void ready(PreTriggerAction action)
		{
			XASSERT(isInitialized(), "not initialized");

			Event &event = events[eventSelector];
			event.reset();
			uint32 readyThreadCount = counter.increment();
			if (readyThreadCount < threadCount)
				event.wait();
			else
			{
				action();
				counter.value = 0;
				eventSelector = (eventSelector + 1) % 2;
				event.set();
			}
		}

		inline bool isInitialized() { return threadCount ? true : false; }
		inline uint32 getThreadCount() { return threadCount; }
		inline uint32 getReadyThreadCount() { return counter.load(); }
	};

	template <>
	class Barrier<BarrierBlockMode::CyclicLocking, BarrierTriggerType::External> : public NonCopyable
	{
	private:

	public:
		inline void wait()
		{
			XASSERT(isInitialized(), "not initialized");
		}
		inline void trigger()
		{
			XASSERT(isInitialized(), "not initialized");
		}

		inline bool isInitialized() { }
	};

	/*template <>
	class Barrier<BarrierType::CyclicLockFree> : public NonCopyable
	{
	private:
		Atomic<uint32> threadCounter;
		uint32 threadCount;
		bool globalPhase;

	public:
		class ThreadToken
		{
			friend LockFreeCyclicBarrier;

		private:
			bool phase;

			inline ThreadToken(bool _phase) : phase(_phase) {}
		};

		inline LockFreeCyclicBarrier(uint32 _threadCount = 0) : threadCounter(0), threadCount(_threadCount), globalPhase(false) {}
		inline void initialize(uint32 _threadCount)
		{
			threadCounter.set(0);
			threadCount = _threadCount;
			globalPhase = false;
		}

		inline ThreadToken getThreadToken() { return ThreadToken(!globalPhase); }

		template <typename CallbackType>
		inline void wait(ThreadToken& threadToken, CallbackType callback)
		{
			uint32 readyThreadCount = threadCounter.increment();
			if (readyThreadCount < threadCount)
			{
				while (globalPhase != threadToken.phase);
			}
			else
			{
				callback();
				threadCounter.set(0);
				globalPhase = threadToken.phase;
			}
			threadToken.phase = !globalPhase;
		}
	};*/
}