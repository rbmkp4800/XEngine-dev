#pragma once

#include "XLib.h"

namespace XLib
{
	using TimerRecord = uint64;

	class Timer abstract final
	{
	public:
		static TimerRecord GetRecord();
		static float32 GetTimeDelta(TimerRecord record1, TimerRecord record2 = Timer::GetRecord());
		static uint32 GetCurrentTimeMs();

		static inline float32 GetTimeDelta_UpdateRecord(TimerRecord& record)
		{
			TimerRecord newRecord = GetRecord();
			float32 delta = GetTimeDelta(record, newRecord);
			record = newRecord;
			return delta;
		}
	};
}