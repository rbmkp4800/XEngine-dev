#pragma once

#include "XLib.h"

namespace XLib
{
#if 0
	template <typename T>
	concept CharStreamReader = requires(T reader)
	{
		//{ reader.read() } -> IsTypeEqual<void>;
		{ reader.peek() } -> IsTypeEqual<char>;
		{ reader.get() } -> IsTypeEqual<char>;
		{ reader.isEndOfStream() } -> IsTypeEqual<bool>;
	};

	template <typename T>
	concept CharStreamWriter = requires(T writer)
	{
		//{ writer.write() } -> IsTypeEqual<void>;
		{ writer.put() } -> IsTypeEqual<void>;
	};
#endif


	class MemoryCharStreamReader
	{
	private:
		const char* begin = nullptr;
		const char* end = nullptr;
		const char* current = nullptr;

	public:
		MemoryCharStreamReader() = default;
		~MemoryCharStreamReader() = default;

		inline void initialize(const char* data, uintptr length) { begin = data; end = data + length; current = data; }

		char peek() const { return isEndOfStream() ? *current : 0; }
		char get() { return isEndOfStream() ? *current++ : 0; }
		bool isEndOfStream() const { return current != end && *current != 0; }

	};

	class MemoryCharStreamWriter
	{

	};


	class FileCharStreamReader
	{

	};

	class FileCharStreamWriter
	{

	};


	class DgbOutCharStreamWriter;

	class VirtualCharStreamReader;
	class VirtualCharStreamWriter;

	FileCharStreamReader& GetStdInStream();
	FileCharStreamWriter& GetStdOutStream();
	FileCharStreamWriter& GetStdErrStream();
	DgbOutCharStreamWriter& GetDbgOutStream();
}
