#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

namespace XEngine::Simulator
{
	struct ArchetypeDesc
	{
		uint32 id;
	};

	struct SimulationDefinition
	{
		const ArchetypeDesc* archetypes;
		uint32 archetypeCount;
	};

	////////////////////////////////////////////////////////////////////////////////////

	// class SimulationViewOperativeState;
	// class SimulationViewOperativeStateDelta;

	enum class SimulationStateHandle : uint16 { Zero = 0, };

	class Simulator : public XLib::NonCopyable
	{
	public:
		Simulator() = default;
		~Simulator() = default;

		void initialize(const SimulationDefinition& simulationDefinition, const void* memoryPool, uintptr memoryPoolSize);
		void destroy();

		SimulationStateHandle deserializeState(const void* data, uintptr size);
		void serializeState(SimulationStateHandle state);

		void dropOperativeState();
		void restoreOperativeState(SimulationStateHandle state);

		SimulationStateHandle tick();

		void releaseState(SimulationStateHandle state);

		//void createView();
		//void destroyView();

		//void advanceView(SimulationViewOperativeState& viewOperativeState,
		//	const SimulationViewOperativeStateDelta& viewOperativeStateDelta);
	};
}
