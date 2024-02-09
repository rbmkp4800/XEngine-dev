#include "XEngine.Simulation.Engine.h"

#include <XEngine.Simulation.Model.h>

using namespace XEngine::Simulation;
using namespace XEngine::Simulation::ModelInternals;

void Engine::tickWave(Wave& wave)
{
	const Archetype& archetype = model->archetypes[wave.archetypeIndex];

	for (uint8 jobIndex = 0; jobIndex < archetype.jobCount; jobIndex++)
	{
		const Job& job = archetype.jobs[jobIndex];

		for (uint8 waveEntityIndex = 0; waveEntityIndex < wave.entityCount; waveEntityIndex++)
		{
			job.executorFunc();
		}
	}
}

StateHandle Engine::tick()
{
	for (uint32 waveIndex = 0; waveIndex < waveCount; waveIndex++)
	{
		tickWave(waves[waveIndex]);
	}
}
