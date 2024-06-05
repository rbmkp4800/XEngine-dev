#include "XLib.h"
#include "XLib.CharStream.h"
#include "XLib.String.h"

namespace XLib
{
	// TODO: We might use this instead of `bool showPlus` in `FmtFormatDecSXX`
	/*enum class FmtPlusMode
	{
		None = 0,
		Plus,
		Space,
	};*/

	enum class FmtFPMode
	{
		Generic = 0,
		Fixed,
		Exp,
	};

	// Fmt format //////////////////////////////////////////////////////////////////////////////////

	enum class FmtFormatStatus : uint8
	{
		Success = 0,
		OutputBufferOverflow,
	};

	struct FmtFormatResult
	{
		FmtFormatStatus status;
		uint8 formattedCharCount;
	};

	FmtFormatResult FmtFormatDecU8(uint8 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 0);
	FmtFormatResult FmtFormatDecU16(uint16 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 0);
	FmtFormatResult FmtFormatDecU32(uint32 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 0);
	FmtFormatResult FmtFormatDecU64(uint64 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 0);

	FmtFormatResult FmtFormatDecS8(sint8 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 0, bool showPlus = false);
	FmtFormatResult FmtFormatDecS16(sint16 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 0, bool showPlus = false);
	FmtFormatResult FmtFormatDecS32(sint32 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 0, bool showPlus = false);
	FmtFormatResult FmtFormatDecS64(sint64 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 0, bool showPlus = false);

	FmtFormatResult FmtFormatHex8(uint8 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 2);
	FmtFormatResult FmtFormatHex16(uint16 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 4);
	FmtFormatResult FmtFormatHex32(uint32 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 6);
	FmtFormatResult FmtFormatHex64(uint64 value, char* buffer, uint8 bufferSize, uint8 lzWidth = 8);

	// TODO: show plus?
	FmtFormatResult FmtFormatDecFP32(float32 value, char* buffer, uint8 bufferSize, uint8 prec, FmtFPMode mode = FmtFPMode::Generic, uint8 lzWidth = 0);
	FmtFormatResult FmtFormatDecFP64(float64 value, char* buffer, uint8 bufferSize, uint8 prec, FmtFPMode mode = FmtFPMode::Generic, uint8 lzWidth = 0);


	// Fmt parse ///////////////////////////////////////////////////////////////////////////////////

	enum class FmtParseStatus : uint8
	{
		Success = 0,
		OutOfRange,
	};

	struct FmtParseResult
	{
		FmtParseStatus status;
		uint8 parsedCharCount;
	};

	FmtParseResult FmtParseDecU8(const char* chars, uintptr charCount, uint8& resultValue);
	FmtParseResult FmtParseDecU16(const char* chars, uintptr charCount, uint16& resultValue);
	FmtParseResult FmtParseDecU32(const char* chars, uintptr charCount, uint32& resultValue);
	FmtParseResult FmtParseDecU64(const char* chars, uintptr charCount, uint64& resultValue);

	FmtParseResult FmtParseDecS8(const char* chars, uintptr charCount, sint8& resultValue, bool allowPlus = false);
	FmtParseResult FmtParseDecS16(const char* chars, uintptr charCount, sint16& resultValue, bool allowPlus = false);
	FmtParseResult FmtParseDecS32(const char* chars, uintptr charCount, sint32& resultValue, bool allowPlus = false);
	FmtParseResult FmtParseDecS64(const char* chars, uintptr charCount, sint64& resultValue, bool allowPlus = false);

	FmtParseResult FmtParseHex8(const char* chars, uintptr charCount, uint8& resultValue);
	FmtParseResult FmtParseHex16(const char* chars, uintptr charCount, uint16& resultValue);
	FmtParseResult FmtParseHex32(const char* chars, uintptr charCount, uint32& resultValue);
	FmtParseResult FmtParseHex64(const char* chars, uintptr charCount, uint64& resultValue);

	FmtParseResult FmtParseDecFP32(const char* chars, uintptr charCount, float32& resultValue);
	FmtParseResult FmtParseDecFP64(const char* chars, uintptr charCount, float64& resultValue);


	// Fmt put /////////////////////////////////////////////////////////////////////////////////////

	template <typename CharStreamWriter> inline void FmtPutDecU8(CharStreamWriter& writer, uint8 value, uint8 lzWidth = 0);
	template <typename CharStreamWriter> inline void FmtPutDecU16(CharStreamWriter& writer, uint16 value, uint8 lzWidth = 0);
	template <typename CharStreamWriter> inline void FmtPutDecU32(CharStreamWriter& writer, uint32 value, uint8 lzWidth = 0);
	template <typename CharStreamWriter> inline void FmtPutDecU64(CharStreamWriter& writer, uint64 value, uint8 lzWidth = 0);

	template <typename CharStreamWriter> inline void FmtPutDecS8(CharStreamWriter& writer, sint8 value, uint8 lzWidth = 0, bool showPlus = false);
	template <typename CharStreamWriter> inline void FmtPutDecS16(CharStreamWriter& writer, sint16 value, uint8 lzWidth = 0, bool showPlus = false);
	template <typename CharStreamWriter> inline void FmtPutDecS32(CharStreamWriter& writer, sint32 value, uint8 lzWidth = 0, bool showPlus = false);
	template <typename CharStreamWriter> inline void FmtPutDecS64(CharStreamWriter& writer, sint64 value, uint8 lzWidth = 0, bool showPlus = false);

	template <typename CharStreamWriter> inline void FmtPutHex8(CharStreamWriter& writer, uint8 value, uint8 lzWidth = 2);
	template <typename CharStreamWriter> inline void FmtPutHex16(CharStreamWriter& writer, uint16 value, uint8 lzWidth = 4);
	template <typename CharStreamWriter> inline void FmtPutHex32(CharStreamWriter& writer, uint32 value, uint8 lzWidth = 8);
	template <typename CharStreamWriter> inline void FmtPutHex64(CharStreamWriter& writer, uint64 value, uint8 lzWidth = 16);

	template <typename CharStreamWriter> inline void FmtPutDecFP32(CharStreamWriter& writer, float32 value);
	template <typename CharStreamWriter> inline void FmtPutDecFP64(CharStreamWriter& writer, float64 value);


	// Fmt get /////////////////////////////////////////////////////////////////////////////////////

	//void FmtGetDecU32();
	//void FmtGetDecFP32();
	//void FmtGetDecFP64();
	//void FmtGetLine();
	//void FmtSkipToNewLine();
	//void FmtSkipWhitespaces();


	// Fmt print ///////////////////////////////////////////////////////////////////////////////////

	template <typename ArgType>
	struct FmtPrintArgHandler {};

	template <typename CharStreamWriter, typename ... FmtArgs>
	inline void FmtPrint(CharStreamWriter& writer, const FmtArgs& ... args);

	template <typename ... FmtArgs>
	inline void FmtPrintStdOut(const FmtArgs& ... fmtArgs);

	template <typename ... FmtArgs>
	inline void FmtPrintStdErr(const FmtArgs& ... fmtArgs);

	template <uint32 BufferSize = 1024, typename ... FmtArgs>
	inline void FmtPrintDbgOut(const FmtArgs& ... fmtArgs);

	template <typename StringType, typename ... FmtArgs>
	inline void FmtPrintStr(StringType& string, const FmtArgs& ... fmtArgs);


	// Fmt print arguments /////////////////////////////////////////////////////////////////////////

	struct FmtArgDecU8
	{
		uint8 value;
		uint8 lzWidth;

		inline FmtArgDecU8(uint8 value, uint8 lzWidth = 0) : value(value), lzWidth(lzWidth) {}
	};

	struct FmtArgDecU16
	{
		uint16 value;
		uint8 lzWidth;

		inline FmtArgDecU16(uint16 value, uint8 lzWidth = 0) : value(value), lzWidth(lzWidth) {}
	};

	struct FmtArgDecU32
	{
		uint32 value;
		uint8 lzWidth;

		inline FmtArgDecU32(uint32 value, uint8 lzWidth = 0) : value(value), lzWidth(lzWidth) {}
	};

	struct FmtArgDecU64
	{
		uint64 value;
		uint8 lzWidth;

		inline FmtArgDecU64(uint64 value, uint8 lzWidth = 0) : value(value), lzWidth(lzWidth) {}
	};


	struct FmtArgDecS8
	{
		sint8 value;
		uint8 lzWidth;
		bool showPlus;

		inline FmtArgDecS8(sint8 value, uint8 lzWidth = 0, bool showPlus = false) : value(value), lzWidth(lzWidth), showPlus(showPlus) {}
	};

	struct FmtArgDecS16
	{
		sint16 value;
		uint8 lzWidth;
		bool showPlus;

		inline FmtArgDecS16(sint16 value, uint8 lzWidth = 0, bool showPlus = false) : value(value), lzWidth(lzWidth), showPlus(showPlus) {}
	};

	struct FmtArgDecS32
	{
		sint32 value;
		uint8 lzWidth;
		bool showPlus;

		inline FmtArgDecS32(sint32 value, uint8 lzWidth = 0, bool showPlus = false) : value(value), lzWidth(lzWidth), showPlus(showPlus) {}
	};

	struct FmtArgDecS64
	{
		sint64 value;
		uint8 lzWidth;
		bool showPlus;

		inline FmtArgDecS64(sint64 value, uint8 lzWidth = 0, bool showPlus = false) : value(value), lzWidth(lzWidth), showPlus(showPlus) {}
	};


	struct FmtArgHex8
	{
		uint8 value;
		uint8 lzWidth;

		inline FmtArgHex8(uint8 value, uint8 lzWidth = 2) : value(value), lzWidth(lzWidth) {}
	};

	struct FmtArgHex16
	{
		uint16 value;
		uint8 lzWidth;

		inline FmtArgHex16(uint16 value, uint8 lzWidth = 4) : value(value), lzWidth(lzWidth) {}
	};

	struct FmtArgHex32
	{
		uint32 value;
		uint8 lzWidth;

		inline FmtArgHex32(uint32 value, uint8 lzWidth = 8) : value(value), lzWidth(lzWidth) {}
	};

	struct FmtArgHex64
	{
		uint64 value;
		uint8 lzWidth;

		inline FmtArgHex64(uint64 value, uint8 lzWidth = 16) : value(value), lzWidth(lzWidth) {}
	};


	struct FmtArgFP32
	{
		float32 value;
	};

	struct FmtArgFP64
	{
		float64 value;
	};


	// Fmt print argument handlers /////////////////////////////////////////////////////////////////

	// Literal characters

	template <>
	struct FmtPrintArgHandler<char>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, char arg);
	};

	template <>
	struct FmtPrintArgHandler<unsigned char>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, unsigned char arg);
	};

	// Liternal numerics

	template <>
	struct FmtPrintArgHandler<uint16>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, uint16 arg) { FmtPrintArgHandler<FmtArgDecU16>::Handle(FmtArgDecU16(arg)); }
	};

	template <>
	struct FmtPrintArgHandler<sint16>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, sint16 arg) { FmtPrintArgHandler<FmtArgDecS16>::Handle(FmtArgDecS16(arg)); }
	};

	template <>
	struct FmtPrintArgHandler<uint32>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, uint32 arg) { FmtPrintArgHandler<FmtArgDecU32>::Handle(FmtArgDecU32(arg)); }
	};

	template <>
	struct FmtPrintArgHandler<sint32>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, sint32 arg) { FmtPrintArgHandler<FmtArgDecS32>::Handle(FmtArgDecS32(arg)); }
	};

	template <>
	struct FmtPrintArgHandler<uint64>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, uint64 arg) { FmtPrintArgHandler<FmtArgDecU64>::Handle(FmtArgDecU64(arg)); }
	};

	template <>
	struct FmtPrintArgHandler<sint64>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, sint64 arg) { FmtPrintArgHandler<FmtArgDecS64>::Handle(FmtArgDecS64(arg)); }
	};

	template <>
	struct FmtPrintArgHandler<float32>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, float32 arg) { FmtPrintArgHandler<FmtArgFP32>::Handle(FmtArgFP32(arg)); }
	};

	template <>
	struct FmtPrintArgHandler<float64>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, float64 arg) { FmtPrintArgHandler<FmtArgFP64>::Handle(FmtArgFP64(arg)); }
	};

	// Strings

	template <>
	struct FmtPrintArgHandler<const char*>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, const char* arg);
	};

	template <uintptr CharArraySize>
	struct FmtPrintArgHandler<char[CharArraySize]>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const char* arg) { FmtPrintArgHandler<const char*>::Handle(writer, arg); ...; /* Do we really need to turn this into cstr? */ }
	};

	template <>
	struct FmtPrintArgHandler<StringView<char>>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const StringView<char>& arg) { writer.write(arg.getData(), arg.getLength()); }
	};

	template <uintptr Capacity, typename CounterType>
	struct FmtPrintArgHandler<InplaceString<char, Capacity, CounterType>>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const StringView<char>& arg) { FmtPrintArgHandler<StringView<char>>::Handle(writer, arg); }
	};

	template <typename CounterType, typename AllocatorType>
	struct FmtPrintArgHandler<DynamicString<char, CounterType, AllocatorType>>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const StringView<char>& arg) { FmtPrintArgHandler<StringView<char>>::Handle(writer, arg); }
	};

	// Fmt args

	template <>
	struct FmtPrintArgHandler<FmtArgDecU8>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgDecU8 arg) { FmtPutDecU8(arg.value, arg.lzWidth); }
	};

	template <>
	struct FmtPrintArgHandler<FmtArgDecU16>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgDecU16 arg) { FmtPutDecU16(arg.value, arg.lzWidth); }
	};

	template <>
	struct FmtPrintArgHandler<FmtArgDecU32>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgDecU32 arg) { FmtPutDecU32(arg.value, arg.lzWidth); }
	};

	template <>
	struct FmtPrintArgHandler<FmtArgDecU64>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgDecU64 arg) { FmtPutDecU64(arg.value, arg.lzWidth); }
	};


	template <>
	struct FmtPrintArgHandler<FmtArgDecS8>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgDecS8 arg) { FmtPutDecS8(arg.value, arg.lzWidth, arg.showPlus); }
	};

	template <>
	struct FmtPrintArgHandler<FmtArgDecS16>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgDecS16 arg) { FmtPutDecS16(arg.value, arg.lzWidth, arg.showPlus); }
	};

	template <>
	struct FmtPrintArgHandler<FmtArgDecS32>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgDecS32 arg) { FmtPutDecS32(arg.value, arg.lzWidth, arg.showPlus); }
	};

	template <>
	struct FmtPrintArgHandler<FmtArgDecS64>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgDecS64 arg) { FmtPutDecS64(arg.value, arg.lzWidth, arg.showPlus); }
	};


	template <>
	struct FmtPrintArgHandler<FmtArgHex8>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgHex8 arg) { FmtPutHex8(arg.value, arg.lzWidth); }
	};

	template <>
	struct FmtPrintArgHandler<FmtArgHex16>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgHex16 arg) { FmtPutHex16(arg.value, arg.lzWidth); }
	};

	template <>
	struct FmtPrintArgHandler<FmtArgHex32>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgHex32 arg) { FmtPutHex32(arg.value, arg.lzWidth); }
	};

	template <>
	struct FmtPrintArgHandler<FmtArgHex64>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, FmtArgHex64 arg) { FmtPutHex64(arg.value, arg.lzWidth); }
	};
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Definitions /////////////////////////////////////////////////////////////////////////////////////

template <typename CharStreamWriter>
inline void XLib::FmtPutDecU8(CharStreamWriter& writer, uint8 value, uint8 lzWidth)
{
	char buffer[16];
	const FmtFormatResult formatResult = FmtFormatDecU8(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}

template <typename CharStreamWriter>
inline void XLib::FmtPutDecU16(CharStreamWriter& writer, uint16 value, uint8 lzWidth)
{
	char buffer[16];
	const FmtFormatResult formatResult = FmtFormatDecU16(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}

template <typename CharStreamWriter>
inline void XLib::FmtPutDecU32(CharStreamWriter& writer, uint32 value, uint8 lzWidth)
{
	char buffer[16];
	const FmtFormatResult formatResult = FmtFormatDecU32(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}

template <typename CharStreamWriter>
inline void XLib::FmtPutDecU64(CharStreamWriter& writer, uint64 value, uint8 lzWidth)
{
	char buffer[24];
	const FmtFormatResult formatResult = FmtFormatDecU64(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}


template <typename CharStreamWriter>
inline void XLib::FmtPutDecS8(CharStreamWriter& writer, sint8 value, uint8 lzWidth, bool showPlus)
{
	char buffer[16];
	const FmtFormatResult formatResult = FmtFormatDecS8(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}

template <typename CharStreamWriter>
inline void XLib::FmtPutDecS16(CharStreamWriter& writer, sint16 value, uint8 lzWidth, bool showPlus)
{
	char buffer[16];
	const FmtFormatResult formatResult = FmtFormatDecS16(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}

template <typename CharStreamWriter>
inline void XLib::FmtPutDecS32(CharStreamWriter& writer, sint32 value, uint8 lzWidth, bool showPlus)
{
	char buffer[16];
	const FmtFormatResult formatResult = FmtFormatDecS32(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}

template <typename CharStreamWriter>
inline void XLib::FmtPutDecS64(CharStreamWriter& writer, sint64 value, uint8 lzWidth, bool showPlus)
{
	char buffer[32];
	const FmtFormatResult formatResult = FmtFormatDecS64(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}


template <typename CharStreamWriter>
inline void XLib::FmtPutHex8(CharStreamWriter& writer, uint8 value, uint8 lzWidth)
{
	char buffer[16];
	const FmtFormatResult formatResult = FmtFormatHex8(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}

template <typename CharStreamWriter>
inline void XLib::FmtPutHex16(CharStreamWriter& writer, uint16 value, uint8 lzWidth)
{
	char buffer[16];
	const FmtFormatResult formatResult = FmtFormatHex16(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}

template <typename CharStreamWriter>
inline void XLib::FmtPutHex32(CharStreamWriter& writer, uint32 value, uint8 lzWidth)
{
	char buffer[16];
	const FmtFormatResult formatResult = FmtFormatHex32(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}

template <typename CharStreamWriter>
inline void XLib::FmtPutHex64(CharStreamWriter& writer, uint64 value, uint8 lzWidth)
{
	char buffer[16];
	const FmtFormatResult formatResult = FmtFormatHex64(value, buffer, sizeof(buffer), lzWidth);
	writer.write(buffer, formatResult.formattedCharCount);
}


////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename CharStreamWriter, typename ... FmtArgs>
inline void XLib::FmtPrint(CharStreamWriter& writer, const FmtArgs& ... args)
{
	((void)FmtPrintArgHandler<FmtArgs>::Handle(writer, args), ...);
}

template <typename ... FmtArgs>
inline void XLib::FmtPrintStdOut(const FmtArgs& ... fmtArgs)
{
	FileCharStreamWriter& stdOutStream = GetStdOutStream();
	FmtPrint(stdOutStream, fmtArgs ...);
	stdOutStream.flush();
}

template <typename ... FmtArgs>
inline void XLib::FmtPrintStdErr(const FmtArgs& ... fmtArgs)
{
	FileCharStreamWriter& stdErrStream = GetStdErrStream();
	FmtPrint(stdErrStream, fmtArgs ...);
	stdErrStream.flush();
}

template <uint32 BufferSize, typename ... FmtArgs>
inline void XLib::FmtPrintDbgOut(const FmtArgs& ... fmtArgs)
{
	char buffer[BufferSize];
	MemoryCharStreamWriter bufferStream(...);
	FmtPrint(bufferStream, fmtArgs ...);
	bufferStream.zeroTerminate();
	Debug::Output(buffer);
}
