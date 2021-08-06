#pragma once

#include "XLib.System.File.h"

namespace XLib
{
	class TextStreamReader
	{
	private:
		const char* current = nullptr;

	public:
		TextStreamReader() = default;
		~TextStreamReader() = default;

		inline TextStreamReader(const char* text);

		inline char get();
		inline char peek();
		inline bool endOfStream();
	};

	class TextStreamWriter
	{
		
	};

	template <typename BaseStreamType>
	class BufferedTextStreamReader
	{
	private:
		BaseStreamType baseStream;

	public:
		BufferedTextStreamReader() = default;
		~BufferedTextStreamReader() = default;

		inline char get();
		inline char peek();
		inline bool endOfStream();

		//inline void reset();
		// Clear buffer and seek file to "current" pos?

		inline BaseStreamType& getBaseStream() { return baseStream; }
	};

	template <typename BaseStreamType>
	class BufferedTextStreamWriter
	{
	
	};

	class TextFileStreamReader : public BufferedTextStreamReader<File>
	{
	public:
		bool open(const char* filename);
	};
}
