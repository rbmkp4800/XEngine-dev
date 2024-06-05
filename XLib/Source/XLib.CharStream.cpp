#include "XLib.CharStream.h"

#include "XLib.System.Threading.Atomics.h"

using namespace XLib;

// Std streams /////////////////////////////////////////////////////////////////////////////////////

static FileCharStreamReader StdInStream;
static FileCharStreamWriter StdOutStream;
static FileCharStreamWriter StdErrStream;

static volatile uint16 StdInStreamInitGuard = 0;
static volatile uint16 StdOutStreamInitGuard = 0;
static volatile uint16 StdErrStreamInitGuard = 0;

FileCharStreamReader& XLib::GetStdInStream()
{
	for (;;)
	{
		const uint16 guardValue = Atomics::Load<uint16>(StdInStreamInitGuard);

		if (guardValue == 2)
			break;

		if (guardValue == 0)
		{
			if (Atomics::CompareExchange<uint16>(StdInStreamInitGuard, 1, 0))
			{
				StdInStream.open(GetStdInFileHandle());
				Atomics::Store<uint16>(StdInStreamInitGuard, 2);
			}
		}
	}

	return StdInStream;
}

FileCharStreamWriter& XLib::GetStdOutStream()
{
	for (;;)
	{
		const uint16 guardValue = Atomics::Load<uint16>(StdOutStreamInitGuard);

		if (guardValue == 2)
			break;

		if (guardValue == 0)
		{
			if (Atomics::CompareExchange<uint16>(StdOutStreamInitGuard, 1, 0))
			{
				StdInStream.open(GetStdOutFileHandle());
				Atomics::Store<uint16>(StdOutStreamInitGuard, 2);
			}
		}
	}

	return StdOutStream;
}

FileCharStreamWriter& XLib::GetStdErrStream()
{
	for (;;)
	{
		const uint16 guardValue = Atomics::Load<uint16>(StdErrStreamInitGuard);

		if (guardValue == 2)
			break;

		if (guardValue == 0)
		{
			if (Atomics::CompareExchange<uint16>(StdErrStreamInitGuard, 1, 0))
			{
				StdInStream.open(GetStdErrFileHandle());
				Atomics::Store<uint16>(StdErrStreamInitGuard, 2);
			}
		}
	}

	return StdErrStream;
}
