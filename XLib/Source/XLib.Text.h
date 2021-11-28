#pragma once

#include "XLib.h"

//bool IsWhitespace(char c) { return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v'; }

namespace XLib
{
	class TextStreamReader
	{
	private:
		const char* current = nullptr;
		const char* limit = 0;

	public:
		TextStreamReader() = default;
		~TextStreamReader() = default;

		inline TextStreamReader(const char* data, uintptr length);

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

	template <typename StringType>
	class StringTextStreamWriter
	{
	private:
		StringType& string;

	public:
		inline StringTextStreamWriter(StringType& string) : string(string) {}
		~StringTextStreamWriter() = default;
	};

	////////////////////////////////////////////////////////////////////////////////////////////////

	enum class TextStreamReadFormatStringResult : uint8
	{
		Success = 0,
		CanNotConsume,
		CanNotAppend,
	};

	// "skip whitespaces" returns true if at least one char consumed
	// "skip to new line" returns true if at least one line consumed (not reached end of stream)
	// "skip/forward to first occurrence" stops at next char

	template <typename StreamType> inline bool TextStreamSkipWritespaces(StreamType& stream);
	template <typename StreamType> inline bool TextStreamSkipToNewLine(StreamType& stream);
	template <typename StreamType> inline bool TextStreamSkipToFirstOccurrence(StreamType& stream, char c); // 
	template <typename StreamType> inline bool TextStreamSkipToFirstOccurrence(StreamType& stream, const char* cstr);

	// TODO: Use logic same to `TextStreamReadFormatStringResult` for all return types

	template <typename InStreamType, typename OutStreamType>
	inline bool TextStreamForwardToFirstOccurrence(InStreamType& stream, char c, OutStreamType& streamForwardTo);

	template <typename InStreamType, typename OutStreamType>
	inline bool TextStreamForwardToFirstOccurrence(InStreamType& stream, StringView str, OutStreamType& streamForwardTo);

	template <typename InStreamType, typename OutStreamType>
	inline bool TextStreamForwardToFirstOccurrence(InStreamType& stream, const char* c, OutStreamType& streamForwardTo);

	template <typename StreamType, typename StringType>
	inline TextStreamReadFormatStringResult TextStreamReadCIdentifier(StreamType& stream, StringType& resultString);

	template <typename StreamType, typename ResultType>
	inline bool TextStreamReadIntDec(StreamType& stream, ResultType& result);

	template <typename StreamType, typename ValueType>
	inline bool TextStreamWriteSIntDec(StreamType& stream, ValueType& result);

	template <typename StreamReaderT, typename ... FmtArgsT>
	inline bool TextStreamReadFmt(StreamReaderT& reader, FmtArgsT ... fmtArgs);

	template <typename StreamWriterT, typename ... FmtArgsT>
	inline bool TextStreamWriteFmt(StreamWriterT& writer, FmtArgsT ... fmtArgs);

	//struct RFmtSkipWS {};
	//struct RFmtSkip2NL {};
	//struct RFmtDec {};

	struct WFmtUDec
	{
		uint64 value;
		inline WFmtUDec(uint64 value) : value(value) {}
		template <typename StreamType> inline bool write(StreamType& stream) const { return TextStreamWriteSIntDec(stream, value); }
	};
}
