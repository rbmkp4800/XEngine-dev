#include "XLib.CharStream.h"

#include "XLib.SystemHeapAllocator.h"

using namespace XLib;


// Std streams /////////////////////////////////////////////////////////////////////////////////////

static FileCharStreamReader StdInStream;
static FileCharStreamWriter StdOutStream;
static FileCharStreamWriter StdErrStream;

FileCharStreamReader& XLib::GetStdInStream()
{
	if (!StdInStream.isOpen())
	{
		// TODO: Thread-safe implementation.
		StdInStream.open(File::GetStdInHandle());
	}
	return StdInStream;
}

FileCharStreamWriter& XLib::GetStdOutStream()
{
	if (!StdOutStream.isOpen())
	{
		// TODO: Thread-safe implementation.
		StdOutStream.open(File::GetStdOutHandle());
	}
	return StdOutStream;
}

FileCharStreamWriter& XLib::GetStdErrStream()
{
	if (!StdErrStream.isOpen())
	{
		// TODO: Thread-safe implementation.
		StdErrStream.open(File::GetStdErrHandle());
	}
	return StdErrStream;
}
