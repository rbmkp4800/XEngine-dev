#pragma once

#include "XLib.h"

//bool IsWhitespace(char c) { return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v'; }

namespace XLib
{
	// Common text readers-writers /////////////////////////////////////////////////////////////////

	class MemoryTextReader
	{
	private:
		const char* current = nullptr;
		const char* limit = nullptr;

	public:
		MemoryTextReader() = default;
		~MemoryTextReader() = default;

		inline MemoryTextReader(const char* data, uintptr length) { current = data; limit = data + length; }

		inline char peekChar() const { return current == limit ? 0 : *current; }
		inline char getChar();
		inline bool canGetChar() const { return current == limit; }

		inline void readChars();
	};

	class MemoryTextWriter
	{
	private:
		char* current = nullptr;
		const char* limit = nullptr;

	public:
		MemoryTextWriter() = default;
		~MemoryTextWriter() = default;

		inline MemoryTextWriter(char* data, uintptr length) { current = data; limit = data + length; }

		inline bool putChar(char c);
		inline bool canPutChar() const;

		inline void writeChars();
	};

	template <typename BaseDataReader, uint32 BufferSize>
	class BufferedTextReader
	{
	private:
		char buffer[BufferSize];
		BaseDataReader& baseReader;

	public:
		inline BufferedTextReader(BaseDataReader& baseReader) : baseReader(baseReader) {}
		~BufferedTextReader() = default;

		inline char peekChar() const;
		inline char getChar();
		inline bool canGetChar() const;

		inline void readChars();
	};

	template <typename BaseWriter>
	class BufferedTextWriter
	{
		inline bool putChar();
		inline bool canPutChar() const;

		inline void writeChars();
	};

	template <typename StringType>
	class StringWriter
	{
	private:
		StringType& string;

	public:
		inline StringWriter(StringType& string) : string(string) {}
		~StringWriter() = default;
	};


	// Text utils //////////////////////////////////////////////////////////////////////////////////

	enum class TextReadTokenResult : uint8
	{
		Success = 0,
		CanNotConsume,
		CanNotAppend,
	};

	// "skip whitespaces" returns true if at least one char consumed
	// "skip to new line" returns true if at least one line consumed (not reached end of stream)
	// "skip/forward to first occurrence" stops at next char

	template <typename TextReader> inline bool TextSkipWhitespaces(TextReader& reader);
	template <typename TextReader> inline bool TextSkipToNewLine(TextReader& reader);
	template <typename TextReader> inline bool TextSkipToFirstOccurrence(TextReader& reader, char c);
	template <typename TextReader> inline bool TextSkipToFirstOccurrence(TextReader& reader, const char* cstr);

	// TODO: Use logic same to `TextReadTokenResult` for all return types

	template <typename TextReader, typename TextWriter>
	inline bool TextForwardToFirstOccurrence(TextReader& reader, char c, TextWriter& writer);

	//template <typename TextReader, typename TextWriter>
	//inline bool TextForwardToFirstOccurrence(TextReader& reader, StringView str, TextWriter& writer);

	template <typename TextReader, typename TextWriter>
	inline bool TextForwardToFirstOccurrence(TextReader& reader, const char* c, TextWriter& writer);


	// Standart types to/from text converstion utils for plain chars ///////////////////////////////
	
	bool TextToInt(const char* text, uintptr textLength, sint64& result);
	bool TextToFP32(const char* text, uintptr textLength, float32& result);

	void TextFromInt(sint64 value, char* outTextBuffer, uintptr outTextBufferLength, char format);
	void TextFromFP32(float32 value, char* outTextBuffer, uintptr outTextBufferLength, char format);


	// Standart types to/from text converstion utils for readers-writers ///////////////////////////

	template <typename TextReader, typename Result>
	inline bool TextReadInt(TextReader& reader, Result& result, char format = 'd');
	template <typename TextReader> inline bool TextReadFP32(TextReader& reader, float32& result);
	template <typename TextReader> inline bool TextReadFP64(TextReader& reader, float64& result);

	template <typename TextWriter> inline bool TextWriteInt(TextWriter& writer, sint64 value, char format = 'd');
	template <typename TextWriter> inline bool TextWriteFP32(TextWriter& writer, float32 value, char format);
	template <typename TextWriter> inline bool TextWriteFP64(TextWriter& writer, float64 value, char format);

	template <typename TextReader, typename ResultString>
	inline TextReadTokenResult TextReadCIdentifier(TextReader& reader, ResultString& result);


	// Formatted text read-write ///////////////////////////////////////////////////////////////////

	template <typename TextReader, typename ... FmtArgs>
	inline bool TextReadFmt(TextReader& reader, FmtArgs ... fmtArgs) { return (... && TextReadFmt_HandleArg(reader, fmtArgs)); }

	template <typename TextWriter, typename ... FmtArgs>
	inline bool TextWriteFmt(TextWriter& writer, FmtArgs ... fmtArgs) { return (... && TextWriteFmt_HandleArg(writer, fmtArgs)); }


	template <typename ... FmtArgs> inline bool TextReadFmtStdIn(FmtArgs ... fmtArgs);
	template <typename ... FmtArgs> inline bool TextWriteFmtStdOut(FmtArgs ... fmtArgs);
	template <typename ... FmtArgs> inline bool TextWriteFmtStdErr(FmtArgs ... fmtArgs);
	template <typename ... FmtArgs> inline bool TextWriteFmtDbgOut(FmtArgs ... fmtArgs);


	struct RFmtSkipWS {};
	struct RFmtSkip2NL {};

	struct RFmtDec
	{

	};

	struct WFmtDec
	{
		uint64 value;
	};

	template <typename TextReader> inline bool TextReadFmt_HandleArg(TextReader& reader, const RFmtSkipWS& arg) { TextSkipWhitespaces(reader); return true; }
	template <typename TextReader> inline bool TextReadFmt_HandleArg(TextReader& reader, const RFmtSkip2NL& arg) { TextSkipToNewLine(reader); return true; }
	template <typename TextReader> inline bool TextReadFmt_HandleArg(TextReader& reader, sint64& arg);
	template <typename TextReader> inline bool TextReadFmt_HandleArg(TextReader& reader, float32& arg);
	template <typename TextReader> inline bool TextReadFmt_HandleArg(TextReader& reader, float64& arg);

	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, char c);
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, const char* str);
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, sint64 arg) { return TextWriteInt(writer, arg); }
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, WFmtDec arg);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	inline char MemoryTextReader::getChar()
	{
		if (current == limit)
			return 0;
		const char result = *current;
		current++;
		return result;
	}

	inline bool MemoryTextWriter::putChar(char c)
	{
		if (current == limit)
			return false;
		*current = c;
		current++;
		return true;
	}


	template <typename TextReader, typename Result>
	inline bool TextReadInt(TextReader& reader, Result& result, char format)
	{
		uint64 r = 0;
		bool atLeastOneDigitConsumed = false;
		for (;;)
		{
			const char c = reader.peekChar();
			if (c < '0' || c > '9')
				break;
			reader.getChar();
			atLeastOneDigitConsumed = true;
			r *= 10;
			r += c - '0';
		}

		result = Result(r);
		return atLeastOneDigitConsumed;
	}

	template <typename TextWriter>
	inline bool TextWriteFmt_HandleArg(TextWriter& writer, const char* str)
	{
		while (*str)
		{
			if (!writer.putChar(*str))
				return false;
			str++;
		}
		return true;
	}
}
