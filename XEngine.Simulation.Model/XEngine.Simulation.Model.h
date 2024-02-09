#include <XLib.h>

namespace XEngine::Simulation
{
	namespace ModelInternals
	{
		struct JobExecutorContext
		{

		};

		using JobExecutorFunc = void(*)(JobExecutorContext& context);

		struct Job
		{
			JobExecutorFunc executorFunc;
		};

		struct Archetype
		{
			uint32 id;

			const Job* jobs;
			uint16 jobCount;
		};
	}

	struct Model
	{
		const ModelInternals::Archetype* archetypes;
		uint32 archetypeCount;
	};
}
