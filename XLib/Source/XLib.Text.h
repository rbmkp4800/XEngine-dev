#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.System.File.h"

// TODO: Pending `TextReader` overhaul.
// TODO: What should `read` return on EOF? 0 or -1?
// TODO: Probably we can replace "read fmt" and "write fmt" with "scan" and "print"? See `java.util.Scanner`.

namespace XLib
{
	// Common text readers-writers /////////////////////////////////////////////////////////////////

	class MemoryTextReader
	{
	private:
		const char* begin = nullptr;
		const char* end = nullptr;
		const char* current = nullptr;

	public:
		MemoryTextReader() = default;
		~MemoryTextReader() = default;

		inline MemoryTextReader(const char* data, uintptr length) { initialize(data, length); }

		inline void initialize(const char* data, uintptr length) { begin = data; end = data + length; current = data; }

		inline bool canGetChar() const { return current != end && *current != 0; }
		inline uint32 peekChar() const { return canGetChar() ? *current : uint32(-1); }
		inline uint32 getChar() { return canGetChar() ? *current++ : uint32(-1); }
		inline char peekCharUnsafe() const { return *current; }
		inline char getCharUnsafe() { return *current++; }
		//inline void readChars();

		inline uintptr getPosition() const { return uintptr(current - begin); }
		inline void setPosition(uintptr position);

		inline const char* getBeginPtr() const { return begin; }
		inline const char* getEndPtr() const { return end; }
		inline const char* getCurrentPtr() const { return current; }
		inline void setCurrentPtr(const char* newCurrentPtr) { XAssert(begin <= newCurrentPtr && newCurrentPtr < end); current = newCurrentPtr; }

		inline uintptr getAvailableBytes() const { return uintptr(end - current); }
	};

	class MemoryTextReaderWithLocation
	{
	private:
		MemoryTextReader base;
		uint32 lineNumber = 0;
		uint32 columnNumber = 0;

	private:
		inline void advanceLocation(uint32 c);

	public:
		MemoryTextReaderWithLocation() = default;
		~MemoryTextReaderWithLocation() = default;

		inline MemoryTextReaderWithLocation(const char* data, uintptr length) { initialize(data, length); }

		inline void initialize(const char* data, uintptr length) { base.initialize(data, length); lineNumber = 0; columnNumber = 0; }

		inline bool canGetChar() const { return base.canGetChar(); }
		inline uint32 peekChar() const { return base.peekChar(); }
		inline uint32 getChar() { const uint32 c = base.getChar(); advanceLocation(c); return c; }
		inline char peekCharUnsafe() const { return base.peekCharUnsafe(); }
		inline char getCharUnsafe() { const uint32 c = base.getCharUnsafe(); advanceLocation(c); return char(c); }

		inline uint32 getLineNumber() const { return lineNumber + 1; }
		inline uint32 getColumnNumber() const { return columnNumber + 1; }
		inline uintptr getAbsoluteOffset() const { return base.getPosition(); }

		inline const char* getBeginPtr() const { return base.getBeginPtr(); }
		inline const char* getEndPtr() const { return base.getEndPtr(); }
		inline const char* getCurrentPtr() const { return base.getCurrentPtr(); }

		inline uintptr getAvailableBytes() const { return base.getAvailableBytes(); }
	};

	class MemoryTextWriter
	{
	private:
		char* buffer = nullptr;
		uint32 bufferSize = 0;
		uint32 length = 0;

	public:
		MemoryTextWriter() = default;
		~MemoryTextWriter() = default;

		inline MemoryTextWriter(char* buffer, uint32 bufferSize) : buffer(buffer), bufferSize(bufferSize) {}

		inline uint32 getLength() const { return length; }

		inline bool append(char c);
		inline bool append(const char* chars, uintptr charCount);
		inline bool canAppend() const { return length + 1 < bufferSize; }
	};

	class FileTextReader : public NonCopyable
	{
	private:
		static constexpr uint32 BufferSize = 4096;

	private:
		char buffer[BufferSize];
		File* file = nullptr;

	public:
		FileTextReader() = default;
		~FileTextReader() = default;

		inline FileTextReader(File& file) : file(&file) {}

		inline void initialize(File& file);

		inline bool canGetChar() const;
		inline uint32 peekChar() const;
		inline uint32 getChar();
		inline char peekCharUnsafe() const;
		inline char getCharUnsafe();
		//inline void readChars();

		inline bool isInitialized() const { return file != nullptr; }
	};

	class FileTextWriter : public NonCopyable
	{
	private:
		static constexpr uint32 BufferSize = 4096;

	private:
		char buffer[BufferSize];
		File* file = nullptr;
		uint16 bufferedCharCount = 0;

	public:
		FileTextWriter() = default;
		~FileTextWriter() = default;

		inline FileTextWriter(File& file) : file(&file) {}

		inline void initialize(File& file);
		inline void flush();
		inline bool isInitialized() const { return file != nullptr; }

		inline bool append(char c);
		inline bool append(const char* chars, uintptr charCount);
		inline bool canAppend() const { return file && file->isInitialized(); }
	};

	class TextWriterVirtualAdapterBase
	{
	public:
		virtual bool append(const char* chars, uintptr charCount) = 0;
	};

	template <typename BaseTextWriter>
	class TextWriterVirtualAdapter
	{
	private:
		BaseTextWriter& baseWriter;

	public:
		TextWriterVirtualAdapter(BaseTextWriter& baseWriter) : baseWriter(baseWriter) {}
		~TextWriterVirtualAdapter() = default;

		virtual bool append(const char* chars, uintptr charCount) override { return baseWriter.append(chars, charCount); }
	};


	// Std IO //////////////////////////////////////////////////////////////////////////////////////

	FileTextReader& GetStdInTextReader();
	FileTextWriter& GetStdOutTextWriter();
	FileTextWriter& GetStdErrTextWriter();


	// Text utils //////////////////////////////////////////////////////////////////////////////////

	class Char abstract final
	{
	public:
		static inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }
		static inline bool IsLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
		static inline bool IsLetterOrDigit(char c) { return IsLetter(c) || IsDigit(c); }
		static inline bool IsLower(char c) { return c >= 'a' && c <= 'z'; }
		static inline bool IsUpper(char c) { return c >= 'A' && c <= 'Z'; }
		static inline bool IsWhitespace(char c) { return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v'; }

		static inline char ToUpper(char c) { return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c; }
		static inline char ToLower(char c) { return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }
	};

	enum class TextReadTokenResult : uint8
	{
		Success = 0,
		CanNotConsume,
		CanNotAppend,
	};

	// "skip whitespaces" returns true if at least one char consumed
	// "skip to new line" returns true if at least one line consumed (not reached end of text)
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
	inline bool TextForwardToFirstOccurrence(TextReader& reader, const char* substring, TextWriter& writer);


	// Standart types to/from text converstion utils for plain chars ///////////////////////////////
	
	bool TextToInt(const char* text, uintptr textLength, sint64& result);
	bool TextToFP32(const char* text, uintptr textLength, float32& result);

	void TextFromInt(char* outTextBuffer, uintptr outTextBufferLength, sint64 value, char format);
	void TextFromFP32(char* outTextBuffer, uintptr outTextBufferLength, float32 value, char format);


	// Standart types to/from text converstion utils for readers-writers ///////////////////////////

	template <typename TextReader, typename Result> inline bool TextReadInt(TextReader& reader, Result* result, char format = 'd');
	template <typename TextReader> inline bool TextReadFP32(TextReader& reader, float32* result);
	template <typename TextReader> inline bool TextReadFP64(TextReader& reader, float64* result);

	template <typename TextWriter> inline bool TextWriteInt(TextWriter& writer, uint64 value, char format = 'd');
	template <typename TextWriter> inline bool TextWriteFP32(TextWriter& writer, float32 value, char format);
	template <typename TextWriter> inline bool TextWriteFP64(TextWriter& writer, float64 value, char format);

	template <typename TextReader, typename ResultString>
	inline bool TextReadCIdentifier(TextReader& reader, ResultString& result);


	// Formatted text read-write ///////////////////////////////////////////////////////////////////

	template <typename TextReader, typename ... FmtArgs>
	inline bool TextReadFmt(TextReader& reader, const FmtArgs& ... fmtArgs) { return (... && TextReadFmt_HandleArg(reader, fmtArgs)); }

	template <typename TextWriter, typename ... FmtArgs>
	inline bool TextWriteFmt(TextWriter& writer, const FmtArgs& ... fmtArgs) { return (... && TextWriteFmt_HandleArg(writer, fmtArgs)); }


	template <typename ... FmtArgs> inline bool TextReadFmtStdIn(const FmtArgs& ... fmtArgs);
	template <typename ... FmtArgs> inline void TextWriteFmtStdOut(const FmtArgs& ... fmtArgs);
	template <typename ... FmtArgs> inline void TextWriteFmtStdErr(const FmtArgs& ... fmtArgs);
	template <typename ... FmtArgs> inline void TextWriteFmtDbgOut(const FmtArgs& ... fmtArgs);


	struct RFmtSkipWS {};
	struct RFmtSkip2NL {};

	class RFmtInt
	{
		template <typename TextReader>
		friend bool TextReadFmt_HandleArg(TextReader& reader, const RFmtInt& arg);

	private:
		void* outValue;
		uint8 outValueSize;
		char format;

	public:
		template <typename T> inline RFmtInt(T* outValue, char format = 'd');
	};

	struct WFmtDec
	{
		uint64 value;
	};

	template <typename TextReader> inline bool TextReadFmt_HandleArg(TextReader& reader, const RFmtSkipWS& arg) { TextSkipWhitespaces(reader); return true; }
	template <typename TextReader> inline bool TextReadFmt_HandleArg(TextReader& reader, const RFmtSkip2NL& arg) { TextSkipToNewLine(reader); return true; }

	template <typename TextReader, typename IntegerArg> inline bool TextReadFmt_HandleArg(TextReader& reader, IntegerArg* arg) { return TextReadInt(reader, arg); }

	template <typename TextReader> inline bool TextReadFmt_HandleArg(TextReader& reader, const RFmtInt& arg);

	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, char c) { return writer.append(c); }
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, unsigned char c) { return writer.append(c); }
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, const char* str);

	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, unsigned short int arg) { return TextWriteInt(writer, arg); }
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, signed short int arg) { return TextWriteInt(writer, arg); }
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, unsigned int arg) { return TextWriteInt(writer, arg); }
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, signed int arg) { return TextWriteInt(writer, arg); }
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, unsigned long int arg) { return TextWriteInt(writer, arg); }
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, signed long int arg) { return TextWriteInt(writer, arg); }
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, unsigned long long int arg) { return TextWriteInt(writer, arg); }
	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, signed long long int arg) { return TextWriteInt(writer, arg); }

	template <typename TextWriter> inline bool TextWriteFmt_HandleArg(TextWriter& writer, WFmtDec arg);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	inline void MemoryTextReaderWithLocation::advanceLocation(uint32 c)
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

	inline bool MemoryTextWriter::append(char c)
	{
		if (!canAppend())
			return false;
		buffer[length] = c;
		length++;
	}

	inline void FileTextReader::initialize(File& file)
	{
		this->file = &file;
	}

	inline void FileTextWriter::initialize(File& file)
	{
		this->file = &file;
		bufferedCharCount = 0;
	}

	inline bool FileTextWriter::append(char c)
	{
		// TODO: Do we need to fail if `file` is null?
		buffer[bufferedCharCount] = c;
		bufferedCharCount++;
		if (bufferedCharCount == BufferSize)
			flush();
		return true;
	}

	inline void FileTextWriter::flush()
	{
		if (file)
			file->write(buffer, bufferedCharCount);
		bufferedCharCount = 0;
	}


	template <typename TextReader>
	inline bool TextSkipWhitespaces(TextReader& reader)
	{
		bool atLeastOneCharConsumed = false;
		while ( Char::IsWhitespace(char(reader.peekChar())) )
		{
			reader.getCharUnsafe();
			atLeastOneCharConsumed = true;
		}
		return atLeastOneCharConsumed;
	}

	template <typename TextReader>
	inline bool TextSkipToNewLine(TextReader& reader)
	{
		while (reader.canGetChar())
		{
			if (reader.getCharUnsafe() == '\n')
				return true;
		}
		return false;
	}

	template <typename TextReader, typename TextWriter>
	inline bool TextForwardToFirstOccurrence(TextReader& reader, const char* substring, TextWriter& writer)
	{
		// TODO: User proper rolling hash.
		// TODO: For now we do not stop if write fails.

		constexpr uint32 selectionBufferSizeLog2 = 10;
		constexpr uint32 selectionBufferSize = 1 << selectionBufferSizeLog2;
		constexpr uint32 selectionBufferIdxMask = selectionBufferSize - 1;

		uintptr substringLength = 0;
		uint32 substringHash = 0;
		while (substring[substringLength])
		{
			substringHash += substring[substringLength];
			substringLength++;
		}

		XAssert(substringLength <= selectionBufferSize);
		if (substringLength > selectionBufferSize)
			return false;

		char selectionBuffer[selectionBufferSize];
		uint32 selectionHash = 0;
		uint32 selectionStartIdx = 0;

		// Fill selection buffer.
		for (uint32 i = 0; i < substringLength; i++)
		{
			if (!reader.canGetChar())
			{
				// Flush selection buffer and quit.
				for (uint32 j = 0; j < i; j++)
					writer.append(selectionBuffer[j]);

				return false;
			}

			const char c = reader.getCharUnsafe();
			selectionHash += c;
			selectionBuffer[i] = c;
		}

		for (;;)
		{
			if (selectionHash == substringHash)
			{
				bool isEqual = true;
				for (uint32 i = 0; i < substringLength; i++)
				{
					if (selectionBuffer[(selectionStartIdx + i) & selectionBufferIdxMask] != substring[i])
					{
						isEqual = false;
						break;
					}
				}

				if (isEqual)
					return true;
			}

			// Move selection and update hash.

			if (!reader.canGetChar())
				break;

			const char charToAdd = reader.getCharUnsafe();
			const char charToRemove = selectionBuffer[selectionStartIdx & selectionBufferIdxMask];

			selectionHash += charToAdd;
			selectionHash -= charToRemove;

			selectionBuffer[(selectionStartIdx + substringLength) & selectionBufferIdxMask] = charToAdd;
			selectionStartIdx++;

			writer.append(charToRemove);
		}

		// If we are here reader is empty and substring is not found. Flush selection buffer and quit.
		for (uint32 i = 0; i < substringLength; i++)
			writer.append(selectionBuffer[(selectionStartIdx + i) & selectionBufferIdxMask]);

		return false;
	}


	namespace Internal
	{
		template <uint32 ResultSize> struct IntSizeToUnsignedType { using T = void; };
		template <> struct IntSizeToUnsignedType<1> { using T = uint8; };
		template <> struct IntSizeToUnsignedType<2> { using T = uint16; };
		template <> struct IntSizeToUnsignedType<4> { using T = uint32; };
		template <> struct IntSizeToUnsignedType<8> { using T = uint64; };

		template <typename TextReader, uint32 ResultSize>
		inline bool TextReadIntHelper(TextReader& reader, void* result, char format)
		{
			using ResultT = typename IntSizeToUnsignedType<ResultSize>::T;

			ResultT r = 0;
			bool atLeastOneDigitConsumed = false;
			for (;;)
			{
				const char c = char(reader.peekChar());
				if (c < '0' || c > '9')
					break;
				reader.getChar();
				atLeastOneDigitConsumed = true;
				r *= 10;
				r += c - '0';
			}

			*(ResultT*)result = ResultT(r);
			return atLeastOneDigitConsumed;
		}
	}

	template <typename TextReader, typename Result>
	inline bool TextReadInt(TextReader& reader, Result* result, char format)
	{
		static_assert(isInt<Result>);
		return Internal::TextReadIntHelper<TextReader, sizeof(*result)>(reader, result, format);
	}

	template <typename TextWriter>
	inline bool TextWriteInt(TextWriter& writer, uint64 value, char format)
	{
		char buffer[24];
		sint8 charCount = 0;
		for (;;)
		{
			buffer[charCount] = value % 10 + '0';
			charCount++;
			value /= 10;
			if (!value)
				break;
		}

		while (charCount)
		{
			charCount--;
			if (!writer.append(buffer[charCount]))
				return false;
		}

		return true;
	}

	template <typename TextReader, typename ResultString>
	inline bool TextReadCIdentifier(TextReader& reader, ResultString& result)
	{
		if (!reader.canGetChar())
			return false;
		const char firstChar = reader.peekCharUnsafe();
		const bool isIdenitfier = Char::IsLetter(firstChar) || firstChar == '_';
		if (!isIdenitfier)
			return false;

		reader.getCharUnsafe();
		result.clear();
		result.pushBack(firstChar);

		for (;;)
		{
			const char c = char(reader.peekChar());
			if (!Char::IsLetterOrDigit(c) && c != '_')
				break;
			reader.getCharUnsafe();
			result.pushBack(c); // TODO: Handle when this fails for `InplaceString`
		}

		return true;
	}


	template <typename ... FmtArgs>
	inline void TextWriteFmtStdOut(const FmtArgs& ... fmtArgs)
	{
		FileTextWriter& stdOutWriter = GetStdOutTextWriter();
		TextWriteFmt(stdOutWriter, fmtArgs ...);
		stdOutWriter.flush();
	}

	template <typename ... FmtArgs>
	inline void TextWriteFmtDbgOut(const FmtArgs& ... fmtArgs)
	{
		char buffer[4096];
		MemoryTextWriter bufferWriter(buffer, sizeof(buffer));
		TextWriteFmt(bufferWriter, fmtArgs ...);
		bufferWriter.append(0);
		Debug::Output(buffer);
	}


	template <typename TextReader>
	inline bool TextReadFmt_HandleArg(TextReader& reader, const RFmtInt& arg)
	{
		*(int*)arg.outValue = 999;
		return true;
	}


	template <typename TextWriter>
	inline bool TextWriteFmt_HandleArg(TextWriter& writer, const char* str)
	{
		while (*str)
		{
			if (!writer.append(*str))
				return false;
			str++;
		}
		return true;
	}
}
