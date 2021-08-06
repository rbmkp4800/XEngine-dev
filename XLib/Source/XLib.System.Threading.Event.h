#pragma once

#include "XLib.System.Threading.h"

namespace XLib
{
	class Event : public WaitableBase
	{
	public:
		Event() = default;
		inline Event(bool state, bool manualReset = true)
		{
			initialize(state, manualReset);
		}

		void initialize(bool state, bool manualReset = true);

		void set();
		void reset();
		void pulse();
	};
}