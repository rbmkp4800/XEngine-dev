#include "XEngine.Utils.CmdLineArgsParser.h"

using namespace XLib;
using namespace XEngine::Utils;

// TODO: Cleanup this mess.

bool CmdLineArgsParser::advance()
{
	auto setCurrentArg = [this](CmdLineArgType type, const char* begin, const char* end, const char* keyValueSeparatorPtr = nullptr) -> void
	{
		currentArgBegin = begin;
		currentArgEnd = end;
		currentArgKeyValueSeparatorPtr = keyValueSeparatorPtr;
		currentArgType = type;
	};

	setCurrentArg(CmdLineArgType::None, nullptr, nullptr);

	while (Char::IsWhitespace(*iterator) && *iterator)
		iterator++;

	if (!*iterator)
		return true;

	const char* argBegin = iterator;

	if (*iterator == '-')
	{
		iterator++;
		for (;;)
		{
			if (Char::IsWhitespace(*iterator) || !*iterator)
			{
				setCurrentArg(CmdLineArgType::Key, argBegin, iterator);
				return true;
			}

			if (*iterator == '\"')
				return false;

			if (*iterator == '=')
				break;

			iterator++;
		}

		const char* argKeyValueSeparatorPtr = iterator;
		iterator++;

		if (*iterator == '\"')
		{
			iterator++;

			for (;;)
			{
				if (*iterator == '\"')
					break;

				if (!*iterator)
				{
					setCurrentArg(CmdLineArgType::None, nullptr, nullptr);
					return false;
				}

				iterator++;
			}

			iterator++;
			if (!Char::IsWhitespace(*iterator) && *iterator)
				return false;

			setCurrentArg(CmdLineArgType::KeyValuePair, argBegin, iterator, argKeyValueSeparatorPtr);
			return true;
		}
		else
		{
			for (;;)
			{
				if (Char::IsWhitespace(*iterator) || !*iterator)
					break;

				if (*iterator == '\"')
					return false;

				iterator++;
			}

			setCurrentArg(CmdLineArgType::KeyValuePair, argBegin, iterator, argKeyValueSeparatorPtr);
			return true;
		}
	}
	else
	{
		if (*iterator == '\"')
		{
			iterator++;

			for (;;)
			{
				if (*iterator == '\"')
					break;

				if (!*iterator)
					return false;

				iterator++;
			}

			iterator++;
			if (!Char::IsWhitespace(*iterator) && *iterator)
				return false;

			setCurrentArg(CmdLineArgType::Value, argBegin, iterator);
			return true;
		}
		else
		{
			for (;;)
			{
				if (Char::IsWhitespace(*iterator) || !*iterator)
					break;

				if (*iterator == '\"')
					return false;

				iterator++;
			}

			setCurrentArg(CmdLineArgType::Value, argBegin, iterator);
			return true;
		}
	}
}

StringViewASCII CmdLineArgsParser::getCurrentArgKey() const
{
	XAssert(currentArgType == CmdLineArgType::Key || currentArgType == CmdLineArgType::KeyValuePair);
	const char* keyEnd = (currentArgType == CmdLineArgType::Key) ? currentArgEnd : currentArgKeyValueSeparatorPtr;
	return StringViewASCII(currentArgBegin, keyEnd);
}

StringViewASCII CmdLineArgsParser::getCurrentArgValue() const
{
	XAssert(currentArgType == CmdLineArgType::Value || currentArgType == CmdLineArgType::KeyValuePair);

	const char* valueBegin = (currentArgType == CmdLineArgType::Value) ? currentArgBegin : (currentArgKeyValueSeparatorPtr + 1);
	const char* valueEnd = currentArgEnd;

	if (*valueBegin == '\"')
	{
		valueBegin++;
		valueEnd--;
		XAssert(*valueEnd == '\"');
	}

	return StringViewASCII(valueBegin, valueEnd);
}
