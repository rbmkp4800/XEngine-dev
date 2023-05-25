#include "XLib.Text.h"

// TODO: Make StdIn, StdOut, StdErr getters thread safe.

using namespace XLib;

namespace
{
	static FileTextReader StdInReader;
	static FileTextWriter StdOutWriter;
	static FileTextWriter StdErrWriter;
}

FileTextReader& XLib::GetStdInTextReader()
{
	if (!StdInReader.isInitialized())
		StdInReader.initialize(File::GetStdIn());
	return StdInReader;
}

FileTextWriter& XLib::GetStdOutTextWriter()
{
	if (!StdOutWriter.isInitialized())
		StdOutWriter.initialize(File::GetStdOut());
	return StdOutWriter;
}

FileTextWriter& XLib::GetStdErrTextWriter()
{
	if (!StdErrWriter.isInitialized())
		StdErrWriter.initialize(File::GetStdErr());
	return StdErrWriter;
}

uintptr XLib::TextConvertASCIIToWide(const char* asciText, wchar* resultWideText, uintptr lengthLimit)
{
	for (uintptr i = 0; i < lengthLimit; i++)
	{
		if (!asciText[i])
			return i;
		resultWideText[i] = wchar(asciText[i]); // T_T
	}
	return lengthLimit;
}

void MemoryTextReaderWithLocation::advanceLocation(uint32 c)
{
	// TODO: Handle end of file properly.

	if (c == '\n')
	{
		lineNumber++;
		columnNumber = 0;
	}
	else if (c != '\r')
	{
		columnNumber++;
	}
}
