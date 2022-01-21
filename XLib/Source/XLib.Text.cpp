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
