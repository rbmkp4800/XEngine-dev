#pragma once

#include "XLib.h"

namespace XLib
{
	class MemoryCharStreamReader;
	class MemoryCharStreamWriter;

	class FileCharStreamReader;
	class FileCharStreamWriter;

	class DgbOutCharStreamWriter;

	class VirtualCharStreamReader;
	class VirtualCharStreamWriter;

	FileCharStreamReader& GetStdInStream();
	FileCharStreamWriter& GetStdOutStream();
	FileCharStreamWriter& GetStdErrStream();
	DgbOutCharStreamWriter GetDbgOutStream();
}
