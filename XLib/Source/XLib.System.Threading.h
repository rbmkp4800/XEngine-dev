#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.Delegate.h"

// TODO: create Thread::destroy();
//       thread fences... lol

namespace XLib
{
	class ISystemHandle abstract : public NonCopyable
	{
	protected:
		void *handle = nullptr;

	public:
		ISystemHandle() = default;
		inline ~ISystemHandle() { destroy(); }

		void destroy();

		inline void* getHandle() { return handle; }
		inline bool isInitialized() { return handle != nullptr; }
	};

	template <typename Type>
	using ThreadMainProc = uint32(__stdcall*)(Type*);

	namespace _private
	{
		bool WaitAll(void** handles, uint32 handleCount);
	}

	class WaitableBase abstract : public ISystemHandle
	{
	public:
		/*template <typename Type>
		static inline void WaitAll(Type *waitableVector, uint32 waitableCount)
		{

		}*/

		bool wait();
		bool wait(uint32 timeout);
	};

	class Thread : public WaitableBase
	{
	private:
		void _create(ThreadMainProc<void> threadMainProc, void* args, bool suspended);

		template <uint32 argsSize>
		struct ArgsStruct { byte data[argsSize]; };

		template <uint32 argsSize>
		struct ThreadMainProcArgs_MethodWithArgsWrapper
		{
			RawDelegate threadMainRawDlegate;
			ArgsStruct<argsSize> *threadMainArgs;
			bool ready;
		};

		template <uint32 argsSize>
		static uint32 __stdcall ThreadMainProc_MethodWithArgsWrapper(
			ThreadMainProcArgs_MethodWithArgsWrapper<argsSize>* args)
		{
			Delegate<void, ArgsStruct<argsSize>> threadMainDelegate(args->threadMainRawDlegate);
			ArgsStruct<argsSize> threadMainArgs = *(args->threadMainArgs);
			args->ready = true;
			threadMainDelegate.call(threadMainArgs);
			return 0;
		}

	public:
		Thread() = default;
		~Thread() = default;

		void terminate(uint32 exitCode = 0);
		void suspend();
		void resume();

		void create(Delegate<void> threadMainDelegate, bool suspended = false);
		void create(Delegate<uint32> threadMainDelegate, bool suspended = false);

		template <typename ArgsType>
		inline void create(ThreadMainProc<ArgsType> threadMainProc,
			ArgsType* args = nullptr, bool suspended = false)
			{ _create(ThreadMainProc<void>(threadMainProc), args, suspended); }

		template <typename ArgsType>
		inline void create(Delegate<void, ArgsType> threadMainDelegate,
			const ArgsType& sourceArguments, bool suspended = false)
		{
			constexpr uint32 argsSize = sizeof(ArgsType);
			ThreadMainProcArgs_MethodWithArgsWrapper<argsSize> args;
			args.threadMainRawDlegate = threadMainDelegate.toRaw();
			args.threadMainArgs = (ArgsStruct<argsSize>*) &sourceArguments;
			args.ready = false;

			create(ThreadMainProc_MethodWithArgsWrapper, &args, suspended);

			while (!args.ready) {}
		}

		static void Sleep(uint32 milliseconds);
		static void YieldExecution();
		static uint32 GetLogicalCoreCount();

		template <typename Type, uint32 count>
		static inline void WaitAll(Type(&waitables)[count], uint32 waitableCount)
		{
			static_assert(sizeof(Type) == sizeof(void*), "Waitable must inherit IWaitable interface, member vars not allowed");
			_private::WaitAll((void**)waitables, waitableCount);
		}
	};
}