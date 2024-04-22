#include "XLib.h"
#include "XLib.CharStream.h"
#include "XLib.String.h"

namespace XLib
{
	struct FmtArgInt
	{
		union
		{
			uint64 uvalue = 0;
			sint64 svalue;
		};

		char fmt = 0;
		uint8 width = 0;
		char leadingCharacter = ' ';

		static inline FmtArgInt SDec(sint64 value) { return FmtArgInt{ .svalue = value, .fmt = 's' }; }
		static inline FmtArgInt UDec(uint64 value) { return FmtArgInt{ .uvalue = value, .fmt = 'u' }; }
		static inline FmtArgInt Hex(uint64 value) { return FmtArgInt{ .uvalue = value, .fmt = 'X' }; }
		static inline FmtArgInt Bin(uint64 value) { return FmtArgInt{ .uvalue = value, .fmt = 'b' }; }
	};

	struct FmtArgFP
	{
		union
		{
			float32 value32;
			float64 value64;
		};

		uint8 width = 0;

	};

	template <typename ArgType>
	struct FmtPrintArgHandler {};

	template <typename CharStreamWriter, typename ... FmtArgs>
	inline void FmtPrint(CharStreamWriter& writer, const FmtArgs& ... args)
		{ ((void)FmtPrintArgHandler<FmtArgs>::Handle(writer, args), ... ); }


	///////////////////////////////////////////////////////////////////////////////////////////////////

	//void FmtScanInt();
	//void FmtScanFP();
	//void FmtReadLine();
	//void FmtSkipToNewLine();
	//void FmtSkipWhitespaces();
	//void FmtSkipWhitespacesOrNewLines();


	///////////////////////////////////////////////////////////////////////////////////////////////////

	//template <typename ... FmtArgs> inline bool FmtScanStdIn(const FmtArgs& ... fmtArgs);
	template <typename ... FmtArgs> inline void FmtPrintStdOut(const FmtArgs& ... fmtArgs);
	template <typename ... FmtArgs> inline void FmtPrintStdErr(const FmtArgs& ... fmtArgs);
	template <typename ... FmtArgs> inline void FmtPrintDbgOut(const FmtArgs& ... fmtArgs);


	// FmtPrint arg handlers //////////////////////////////////////////////////////////////////////////

	template <>
	struct FmtPrintArgHandler<char>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, char arg);
	};

	template <>
	struct FmtPrintArgHandler<unsigned char>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, unsigned char arg);
	};

	template <>
	struct FmtPrintArgHandler<uint16>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, uint16 arg);
	};

	template <>
	struct FmtPrintArgHandler<sint16>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, sint16 arg);
	};

	template <>
	struct FmtPrintArgHandler<uint32>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, uint32 arg);
	};

	template <>
	struct FmtPrintArgHandler<sint32>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, sint32 arg);
	};

	template <>
	struct FmtPrintArgHandler<uint64>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, uint64 arg);
	};

	template <>
	struct FmtPrintArgHandler<sint64>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, sint64 arg);
	};

	template <>
	struct FmtPrintArgHandler<float32>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, float32 arg);
	};

	template <>
	struct FmtPrintArgHandler<float64>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, float64 arg);
	};

	template <>
	struct FmtPrintArgHandler<const char*>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const char* arg);
	};

	template <>
	struct FmtPrintArgHandler<FmtArgInt>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const FmtArgInt& arg);
	};

	template <>
	struct FmtPrintArgHandler<FmtArgFP>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const FmtArgFP& arg);
	};

	template <typename CharType>
	struct FmtPrintArgHandler<StringView<CharType>>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const StringView<CharType>& arg);
	};

	template <typename CharType, uintptr Capacity, typename CounterType>
	struct FmtPrintArgHandler<InplaceString<CharType, Capacity, CounterType>>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const StringView<CharType>& arg);
	};

	template <typename CharType, typename CounterType, typename AllocatorType>
	struct FmtPrintArgHandler<DynamicString<CharType, CounterType, AllocatorType>>
	{
		template <typename CharStreamWriter>
		static inline void Handle(CharStreamWriter& writer, const StringView<CharType>& arg);
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{

}
