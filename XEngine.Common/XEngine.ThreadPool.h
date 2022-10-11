#pragma once

#include <XLib.h>

namespace XEngine
{
	class ThreadPool abstract final
	{
	public:
		using OccupantRoutine = void(*)(void*);

		class ExternalWakeToken
		{
			void signal();
		};

		template <typename ConcreteTask>
		using TaskRoutine = void(*)(ConcreteTask& task);

		class Task abstract
		{
			friend ThreadPool;

		private:
			TaskRoutine<Task> routine = nullptr;

		public:
			Task() = default;
			~Task() = default;
		};

	private:
		static void WorkerThreadMain(uint32 threadIndex);
		static void QueueTaskInternal(Task& task, TaskRoutine<Task> routine);

	public:
		static void Run(uint32 initialThreadCount, OccupantRoutine mainRoutine, void* mainRoutineArg);

		static void SetThreadCount(uint32 threadCount);

		static void OccupyThread(OccupantRoutine routine, void* routineArg, const char* occupantName);
		static void OccupantWaitForConditionBusyLoop();
		static void OccupantWaitForConditionExternalWake(ExternalWakeToken& externalWakeToken);
		static void OccupantSleepPrecise();

		template <typename ConcreteTask>
		static inline void QueueTask(ConcreteTask& task, TaskRoutine<ConcreteTask> routine)
		{
			QueueTaskInternal(task, TaskRoutine<Task>(routine));
		}
		//void QueueTaskAtTimePoint();
	};
}
