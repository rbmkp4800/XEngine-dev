#pragma once

#include "XLib.h"

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

	template <typename BaseStreamType>
	class FormatReader
	{
	private:
		BaseStreamType& baseStream;

	public:
		FormatReader(BaseStreamType& baseStream) : baseStream(baseStream) {}
		~FormatReader() = default;

		inline char get() { return baseStream.get(); }
		inline char peek() { return baseStream.peek(); }
		inline bool endOfStream() { return baseStream.endOfStream(); }

		bool skipWhitespaces(); // returns true if at least one char consumed
		bool skipToNewLine(); // returns true if at least one line consumed (not reached end of stream)
		bool skipToChar(char c); // stops at next char. returns true if at least one char consumed

		template <typename T> bool readBinInt(T& result);
		template <typename T> bool readOctInt(T& result);
		template <typename T> bool readDecInt(T& result);
		template <typename T> bool readHexInt(T& result);

		bool readF32(float32& result);
		bool readF64(float32& result);
	};

#if 0
	class TextFileStreamReader : public BufferedTextStreamReader<File>
	{
	public:
		bool open(const char* filename);
	};

	template <typename StreamReaderT, typename ... FmtArgsT>
	inline void TextFormatRead(StreamReaderT& reader, FmtArgsT ... fmtArgs);

	template <typename StreamWriterT, typename ... FmtArgsT>
	inline void TextFormatWrite(StreamWriterT& writer, FmtArgsT ... fmtArgs);

	namespace Fmt
	{
		class RdToNL
		{

		};

		class RdWS
		{
		public:
			RdWS(bool atLeastOne = false);
			~RdWS() = default;
		};

		class RdS32
		{
		private:
			sint32& result;

		public:
			RdS32(sint32& result);
			~RdS32() = default;

			template <typename Reader>
			static bool Read(Reader& reader, RdS32& result);
		};

		template <typename ResultStringWriter>
		class RdStr
		{
		private:
			ResultStringWriter& resultWriter;

		public:
			RdStr(ResultStringWriter& resultWriter);
			~RdStr() = default;
		};
	}
#endif

}
