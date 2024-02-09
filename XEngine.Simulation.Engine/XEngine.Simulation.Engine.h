#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

namespace XEngine::Simulation
{
	class Model;

	// class ViewOperativeState;
	// class ViewOperativeStateDelta;

	enum class StateHandle : uint16 { Zero = 0, };

	class Engine : public XLib::NonCopyable
	{
	private:
		static constexpr uint32 PageSize = 1024 * 16;

	private:
		struct WaveEntityRecord
		{
			uint64 entityId;
		};

		struct Page
		{

		};

		struct Wave
		{
			uint16 archetypeIndex;

			WaveEntityRecord* entityRecords;
			uint8 entityCount;

		};

	private:
		const Model* model = nullptr;

		Wave* waves = nullptr;
		uint32 waveCount = 0;

	private:
		void tickWave(Wave& wave);

	public:
		Engine() = default;
		~Engine() = default;

		void initialize(const Model& model, const void* memoryPool, uintptr memoryPoolSize);
		void destroy();

		StateHandle deserializeState(const void* data, uintptr size);
		void serializeState(StateHandle state);

		void dropOperativeState();
		void resetOperativeState(StateHandle state);

		StateHandle tick();

		void releaseState(StateHandle state);

		//void createView();
		//void destroyView();

		//void advanceView(SimulationViewOperativeState& viewOperativeState,
		//	const SimulationViewOperativeStateDelta& viewOperativeStateDelta);
	};

	// class NetworkServer { };
	// class NetworkClient { };
}
