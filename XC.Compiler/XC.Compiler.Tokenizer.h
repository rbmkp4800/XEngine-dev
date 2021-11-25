#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

namespace XC::Compiler
{
	enum class TokenCode : uint8
	{
		Zero = 0,

	};

	struct Token
	{
		TokenCode code;
		uint32 lineNumber;
		uint32 columnNumber;
	};

	class TokenStream : public XLib::NonCopyable
	{
	private:

	public:
		TokenStream() = default;
		~TokenStream() = default;

		void tokenize(const char* text, uint64 length = uint64(-1));
	};

	class TokenStreamReader
	{
	private:
		TokenStream& stream;

	public:
		TokenStreamReader() = default;
		~TokenStreamReader() = default;

		Token getNextToken();
	};
}
