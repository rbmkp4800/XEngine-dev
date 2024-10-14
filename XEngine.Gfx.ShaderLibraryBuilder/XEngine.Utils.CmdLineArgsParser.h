#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>

namespace XEngine::Utils
{
	enum class CmdLineArgType : uint8
	{
		None = 0,
		Key,			// `-xxx`
		Value,			// `xxx` / ` "xxx" `
		KeyValuePair,	// `-xxx=yyy` / ` -xxx="yyy" `
	};

	class CmdLineArgsParser : public XLib::NonCopyable
	{
	private:
		const char* iterator;

		const char* currentArgBegin = nullptr;
		const char* currentArgEnd = nullptr;
		const char* currentArgKeyValueSeparatorPtr = nullptr;
		CmdLineArgType currentArgType = {};

	public:
		inline CmdLineArgsParser(const char* cmdLine) : iterator(cmdLine) {}
		~CmdLineArgsParser() = default;

		bool advance();

		inline CmdLineArgType getCurrentArgType() const { return currentArgType; }
		inline XLib::StringViewASCII getCurrentArgRawString() const { return XLib::StringViewASCII(currentArgBegin, currentArgEnd); }

		XLib::StringViewASCII getCurrentArgKey() const;
		XLib::StringViewASCII getCurrentArgValue() const;
	};
}
