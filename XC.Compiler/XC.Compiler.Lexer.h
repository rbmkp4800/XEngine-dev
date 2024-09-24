#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>

namespace XC::Compiler
{
	enum class LexemeType : uint8
	{
		Zero = 0,

	};

	struct Lexeme
	{
		LexemeType type;
		uint32 lineNumber;
		uint32 columnNumber;
	};

	class LexemeStream : public XLib::NonCopyable
	{

	};
}
