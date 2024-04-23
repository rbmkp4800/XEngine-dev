#include "XLib.h"
#include "XLib.CharStream.h"
#include "XLib.String.h"

namespace XLib
{
	void FmtFormatUInt(uint32 value, char* buffer, uint8 bufferSize, uint8& resultLength);
	void FmtFormatSInt(sint32 value, char* buffer, uint8 bufferSize, uint8& resultLength);
	void FmtFormatUInt(uint64 value, char* buffer, uint8 bufferSize, uint8& resultLength);
	void FmtFormatSInt(sint64 value, char* buffer, uint8 bufferSize, uint8& resultLength);
	void FmtFormatFloat(float32 value, char* buffer, uint8 bufferSize, uint8& resultLength);
	void FmtFormatFloat(float32 value, char* buffer, uint8 bufferSize, uint8& resultLength);

	bool FmtParseUInt(const StringViewASCII& string, uint8& result);
	bool FmtParseSInt(const StringViewASCII& string, sint8& result);
	bool FmtParseUInt(const StringViewASCII& string, uint16& result);
	bool FmtParseSInt(const StringViewASCII& string, sint16& result);
	bool FmtParseUInt(const StringViewASCII& string, uint32& result);
	bool FmtParseSInt(const StringViewASCII& string, sint32& result);
	bool FmtParseUInt(const StringViewASCII& string, uint64& result);
	bool FmtParseSInt(const StringViewASCII& string, sint64& result);
	bool FmtParseFloat(const StringViewASCII& string, float32& result);
	bool FmtParseFloat(const StringViewASCII& string, float64& result);

	////////////////////////////////////////////////////////////////////////////////////////////////


	struct FmtArgUInt
	{
		uint32 value = 0;
		char fmt = 0;
		uint8 width = 0;
		bool lz = false;

		static inline FmtArgUInt Dec(uint32 value) { return FmtArgUInt{ .value = value, .fmt = 'u' }; }
		static inline FmtArgUInt Hex(uint32 value) { return FmtArgUInt{ .value = value, .fmt = 'X' }; }
		static inline FmtArgUInt Bin(uint32 value) { return FmtArgUInt{ .value = value, .fmt = 'b' }; }
	};

	struct FmtArgSInt
	{
		sint32 value = 0;
		uint8 width = 0;
		bool lz = false;
	};

	struct FmtArgFP32
	{
		float32 value = 0.0f;
		uint8 width = 0;

	};

	struct FmtArgFP64
	{
		float64 value = 0.0;
		uint8 width = 0;
	};

	template <typename ArgType>
	struct FmtPrintArgHandler {};

	template <typename CharStreamWriter, typename ... FmtArgs>
	inline void FmtPrint(CharStreamWriter& writer, const FmtArgs& ... args);


	////////////////////////////////////////////////////////////////////////////////////////////////

	//template <typename ... FmtArgs>
	//inline bool FmtScanStdIn(const FmtArgs& ... fmtArgs);

	template <typename ... FmtArgs>
	inline void FmtPrintStdOut(const FmtArgs& ... fmtArgs);

	template <typename ... FmtArgs>
	inline void FmtPrintStdErr(const FmtArgs& ... fmtArgs);

	template <uint32 BufferSize = 4096, typename ... FmtArgs>
	inline void FmtPrintDbgOut(const FmtArgs& ... fmtArgs);


	////////////////////////////////////////////////////////////////////////////////////////////////

	//void FmtScanInt();
	//void FmtScanFP();
	//void FmtScanLine();
	//void FmtSkipToNewLine();
	//void FmtSkipWhitespaces();
	//void FmtSkipWhitespacesOrNewLines();


	// FmtPrint arg handlers ///////////////////////////////////////////////////////////////////////

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

	template <>
	struct FmtPrintArgHandler<uint16>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, uint16 arg);
	};

	template <>
	struct FmtPrintArgHandler<sint16>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, sint16 arg);
	};

	template <>
	struct FmtPrintArgHandler<uint32>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, uint32 arg);
	};

	template <>
	struct FmtPrintArgHandler<sint32>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, sint32 arg);
	};

	template <>
	struct FmtPrintArgHandler<uint64>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, uint64 arg);
	};

	template <>
	struct FmtPrintArgHandler<sint64>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, sint64 arg);
	};

	template <>
	struct FmtPrintArgHandler<float32>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, float32 arg);
	};

	template <>
	struct FmtPrintArgHandler<float64>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, float64 arg);
	};

	template <>
	struct FmtPrintArgHandler<const char*>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, const char* arg);
	};

	template <typename CharType>
	struct FmtPrintArgHandler<StringView<CharType>>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, const StringView<CharType>& arg) { writer.write(arg.getData(), arg.getLength()); }
	};

	template <typename CharType, uintptr Capacity, typename CounterType>
	struct FmtPrintArgHandler<InplaceString<CharType, Capacity, CounterType>>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const StringView<CharType>& arg) { FmtPrintArgHandler<StringView<CharType>>::Handle(writer, arg); }
	};

	template <typename CharType, typename CounterType, typename AllocatorType>
	struct FmtPrintArgHandler<DynamicString<CharType, CounterType, AllocatorType>>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const StringView<CharType>& arg) { FmtPrintArgHandler<StringView<CharType>>::Handle(writer, arg); }
	};

	template <>
	struct FmtPrintArgHandler<FmtArgUInt>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, const FmtArgUInt& arg);
	};

	template <>
	struct FmtPrintArgHandler<FmtArgFP32>
	{
		template <typename CharStreamWriter>
		static void Handle(CharStreamWriter& writer, const FmtArgFP32& arg);
	};
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// Definitions /////////////////////////////////////////////////////////////////////////////////////

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

template <uint32 BufferSize = 4096, typename ... FmtArgs>
inline void XLib::FmtPrintDbgOut(const FmtArgs& ... fmtArgs)
{
	char buffer[BufferSize];
	MemoryCharStreamWriter bufferStream(...);
	FmtPrint(bufferStream, fmtArgs ...);
	bufferStream.zeroTerminate();
	Debug::Output(buffer);
}

namespace XLib
{
	template <typename CharStreamWriter>
	void FmtPrintArgHandler<uint16>::Handle(CharStreamWriter& writer, uint16 arg)
	{
		char buffer[16];
		uint8 length = 0;
		FmtFormatUInt(arg, buffer, countof(buffer), length);
		writer.write(buffer, length);
	}

	template <typename CharStreamWriter>
	void FmtPrintArgHandler<sint16>::Handle(CharStreamWriter& writer, sint16 arg)
	{
		char buffer[16];
		uint8 length = 0;
		FmtFormatSInt(arg, buffer, countof(buffer), length);
		writer.write(buffer, length);
	}

	template <typename CharStreamWriter>
	void FmtPrintArgHandler<uint32>::Handle(CharStreamWriter& writer, uint32 arg)
	{
		char buffer[16];
		uint8 length = 0;
		FmtFormatUInt(arg, buffer, countof(buffer), length);
		writer.write(buffer, length);
	}

	template <typename CharStreamWriter>
	void FmtPrintArgHandler<sint32>::Handle(CharStreamWriter& writer, sint32 arg)
	{
		char buffer[16];
		uint8 length = 0;
		FmtFormatSInt(arg, buffer, countof(buffer), length);
		writer.write(buffer, length);
	}

	template <typename CharStreamWriter>
	void FmtPrintArgHandler<uint64>::Handle(CharStreamWriter& writer, uint64 arg)
	{
		char buffer[16];
		uint8 length = 0;
		FmtFormatUInt(arg, buffer, countof(buffer), length);
		writer.write(buffer, length);
	}

	template <typename CharStreamWriter>
	void FmtPrintArgHandler<sint64>::Handle(CharStreamWriter& writer, sint64 arg)
	{
		char buffer[16];
		uint8 length = 0;
		FmtFormatSInt(arg, buffer, countof(buffer), length);
		writer.write(buffer, length);
	}

	template <typename CharStreamWriter>
	void FmtPrintArgHandler<float32>::Handle(CharStreamWriter& writer, float32 arg)
	{
		char buffer[64];
		uint8 length = 0;
		FmtFormatFP(arg, buffer, countof(buffer), length);
		writer.write(buffer, length);
	}

	template <typename CharStreamWriter>
	void FmtPrintArgHandler<float64>::Handle(CharStreamWriter& writer, float64 arg)
	{
		char buffer[64];
		uint8 length = 0;
		FmtFormatFP(arg, buffer, countof(buffer), length);
		writer.write(buffer, length);
	}

	template <typename CharStreamWriter>
	void FmtPrintArgHandler<const char*>::Handle(CharStreamWriter& writer, const char* arg)
	{
		const uintptr length = String::GetCStrLength(arg);
		writer.write(arg, length);
	}
}
