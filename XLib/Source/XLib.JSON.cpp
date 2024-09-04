#include "XLib.Fmt.h"

#include "XLib.JSON.h"

using namespace XLib;

inline bool IsIdentifierStartCharacter(char c) { return Char::IsLetter(c) || c == '_'; }
inline bool IsIdentifierPartCharacter(char c) { return Char::IsLetterOrDigit(c) || c == '_'; }
inline bool IsNumberStartCharacter(char c) { return Char::IsDigit(c) || c == '+' || c == '-' || c == '.'; }

enum class JSONReader::State : uint8
{
	Undefined = 0,
	PendingValue,
	PendingKey,
	PendingObjectScopeBegin,
	PendingObjectScopeEnd,
	PendingArrayScopeBegin,
	PendingArrayScopeEnd,
	EndOfDocumentReached,
};

bool JSONReader::tryConsumeChar(char c)
{
	if (charsReader.peek() != c)
		return false;
	charsReader.get();
	return true;
}

bool JSONReader::skipWhitespaceAndComments()
{
	for (;;)
	{
		FmtSkipWhitespaces(charsReader);

		if (charsReader.peek() != '/')
			break;

		if (charsReader.peek(1) == '/')
		{
			FmtSkipToNewLine(charsReader);
		}
		else if (charsReader.peek(1) == '*')
		{
			charsReader.get();
			charsReader.get();

			for(;;)
			{
				if (charsReader.get() == '*' && charsReader.peek() == '/')
				{
					charsReader.get();
					break;
				}
				if (charsReader.isEndOfStream())
				{
					errorCode = JSONErrorCode::UnexpectedEndOfFile;
					return false;
				}
			}
		}
		else
			break;
	}

	return true;
}

bool JSONReader::parseString(JSONString& result)
{
	const char openingDoubleQuote = charsReader.get();
	XAssert(openingDoubleQuote == '\"');

	const char* stringBeginPtr = charsReader.getCurrentPtr();

	uintptr unescapedLength = 0;
	bool isEscaped = false;
	for (;;)
	{
		if (charsReader.isEndOfStream())
		{
			errorCode = JSONErrorCode::UnexpectedEndOfFile;
			return false;
		}

		const char c = charsReader.peek();

		if (c == '\"')
			break;

		if (c == '\\')
		{
			charsReader.get();

			const char e = charsReader.peek();
			if (e == '\"' || e == '\\' || e == '/' || e == 'b' || e == 'f' || e == 'n' || e == 'r' || e == 't')
			{
				charsReader.get();
			}
			else if (e == 'u')
			{
				errorCode = JSONErrorCode::UnicodeSupportNotImplemented;
				return false;
			}
			else
			{
				errorCode = JSONErrorCode::InvalidStringEscape;
				return false;
			}

			isEscaped = true;
		}
		else if (c < 0x20)
		{
			errorCode = JSONErrorCode::InvalidCharacter;
			return false;
		}
		else if (c >= 128)
		{
			errorCode = JSONErrorCode::UnicodeSupportNotImplemented;
			return false;
		}
		else
		{
			charsReader.get();
		}

		unescapedLength++;
	}

	const char* stringEndPtr = charsReader.getCurrentPtr();

	const char closingDoubleQuote = charsReader.get();
	XAssert(closingDoubleQuote == '\"');

	result.string = StringViewASCII(stringBeginPtr, stringEndPtr);
	result.unescapedLength = unescapedLength;
	result.isEscaped = isEscaped;

	XAssert(imply(!isEscaped, result.string.getLength() == unescapedLength));

	return true;
}

bool JSONReader::parseIdentifier(StringViewASCII& result)
{
	const char* identifierBeginPtr = charsReader.getCurrentPtr();
	const char identifierStartChar = charsReader.get();
	XAssert(IsIdentifierStartCharacter(identifierStartChar));
	while (IsIdentifierPartCharacter(charsReader.peek()))
		charsReader.get();
	const char* identifierEndPtr = charsReader.getCurrentPtr();

	result = StringViewASCII(identifierBeginPtr, identifierEndPtr);
	return true;
}

bool JSONReader::parseNumber(JSONNumber& result)
{
	XAssertNotImplemented();
	return false;
}

bool JSONReader::parseKey(JSONString& result)
{
	const char c = charsReader.peek();
	if (c == '\"')
	{
		if (!parseString(result))
			return false;
	}
	else if (IsIdentifierStartCharacter(c))
	{
		StringViewASCII str = {};
		if (!parseIdentifier(str))
			return false;

		result.string = str;
		result.unescapedLength = str.getLength();
		result.isEscaped = false;
	}
	else
	{
		errorCode = JSONErrorCode::InvalidKeyFormat;
		return false;
	}

	return true;
}

bool JSONReader::parseLiteralValue(JSONValue& result)
{
	const char c = charsReader.peek();
	if (c == '\"')
	{
		if (!parseString(result.string))
			return false;
		result.type = JSONValueType::String;
	}
	else if (IsIdentifierStartCharacter(c))
	{
		StringViewASCII str = {};
		if (!parseIdentifier(str))
			return false;

		if (str == "true")
		{
			result.type = JSONValueType::Boolean;
			result.boolean = true;
		}
		else if (str == "false")
		{
			result.type = JSONValueType::Boolean;
			result.boolean = false;
		}
		else if (str == "null")
		{
			result.type = JSONValueType::Null;
		}
		else
		{
			result.type = JSONValueType::String;
			result.string.string = str;
			result.string.unescapedLength = str.getLength();
			result.string.isEscaped = false;
		}
	}
	else if (IsNumberStartCharacter(c))
	{
		if (!parseNumber(result.number))
			return false;
		result.type = JSONValueType::Number;
	}
	else
	{
		errorCode = JSONErrorCode::InvalidValueFormat;
		return false;
	}

	return true;
}

bool JSONReader::consumeCommaAndSkipToNextElementOrPrepareScopeClose()
{
	if (!skipWhitespaceAndComments())
		return false;

	const bool isRootScope = nestingStackSize == 0;
	if (isRootScope)
	{
		if (!charsReader.isEndOfStream())
		{
			errorCode = JSONErrorCode::UnexpectedTextAfterRootElement;
			return false;
		}
		state = State::EndOfDocumentReached;
		return true;
	}

	// Not root scope.

	if (charsReader.isEndOfStream())
	{
		errorCode = JSONErrorCode::UnexpectedEndOfFile;
		return false;
	}

	const bool commaConsumed = tryConsumeChar(',');
	if (!skipWhitespaceAndComments())
		return false;

	XAssert(nestingStackSize > 0 && nestingStackSize <= 64);
	const bool currentScopeIsArray = nestingStackBits.isSet(nestingStackSize - 1);

	if (charsReader.peek() == (currentScopeIsArray ? ']' : '}'))
		state = currentScopeIsArray ? State::PendingArrayScopeEnd : State::PendingObjectScopeEnd;
	else if (commaConsumed)
		state = currentScopeIsArray ? State::PendingValue : State::PendingKey;
	else
	{
		errorCode = currentScopeIsArray ?
			JSONErrorCode::ExpectedCommaOrSquareBracket : JSONErrorCode::ExpectedCommaOrCurlyBracket;
		return false;
	}

	return true;
}

bool JSONReader::openDocument(const char* text, uintptr length)
{
	nestingStackSize = 0;
	state = State::Undefined;
	errorCode = JSONErrorCode::Success;

	charsReader.open(text, length);
	if (!skipWhitespaceAndComments())
		return false;

	state = State::PendingValue;
	return true;
}

bool JSONReader::readKey(JSONString& result)
{
	XAssert(errorCode == JSONErrorCode::Success);
	XAssert(state == State::PendingKey);

	if (charsReader.isEndOfStream())
	{
		errorCode = JSONErrorCode::UnexpectedEndOfFile;
		return false;
	}
	if (!parseKey(result))
		return false;
	if (!skipWhitespaceAndComments())
		return false;
	if (charsReader.isEndOfStream())
	{
		errorCode = JSONErrorCode::UnexpectedEndOfFile;
		return false;
	}
	if (!tryConsumeChar(':'))
	{
		errorCode = JSONErrorCode::ExpectedColon;
		return false;
	}
	if (!skipWhitespaceAndComments())
		return false;

	state = State::PendingValue;
	return true;
}

bool JSONReader::readValue(JSONValue& result)
{
	XAssert(errorCode == JSONErrorCode::Success);
	XAssert(state == State::PendingValue);

	if (charsReader.isEndOfStream())
	{
		errorCode = JSONErrorCode::UnexpectedEndOfFile;
		return false;
	}

	if (charsReader.peek() == '{')
	{
		result.type = JSONValueType::Object;
		state = State::PendingObjectScopeBegin;
		return true;
	}
	if (charsReader.peek() == '[')
	{
		result.type = JSONValueType::Array;
		state = State::PendingArrayScopeBegin;
		return true;
	}

	if (!parseLiteralValue(result))
		return false;

	return consumeCommaAndSkipToNextElementOrPrepareScopeClose();
}

bool JSONReader::openObject()
{
	XAssert(errorCode == JSONErrorCode::Success);
	XAssert(state == State::PendingObjectScopeBegin);

	if (nestingStackSize >= 64)
	{
		errorCode = JSONErrorCode::MaximumNestingDepthReached;
		return false;
	}
	nestingStackBits.reset(nestingStackSize);
	nestingStackSize++;

	const char leftCurlyBracket = charsReader.get();
	XAssert(leftCurlyBracket == '{');

	if (!skipWhitespaceAndComments())
		return false;

	state = charsReader.peek() == '}' ? State::PendingObjectScopeEnd : State::PendingKey;
	return true;
}

bool JSONReader::closeObject()
{
	XAssert(errorCode == JSONErrorCode::Success);
	XAssert(state == State::PendingObjectScopeEnd);

	XAssert(nestingStackSize > 0);
	XAssert(!nestingStackBits.isSet(nestingStackSize - 1));
	nestingStackSize--;

	const char rightCurlyBracket = charsReader.get();
	XAssert(rightCurlyBracket == '}');

	return consumeCommaAndSkipToNextElementOrPrepareScopeClose();
}

bool JSONReader::openArray()
{
	XAssert(errorCode == JSONErrorCode::Success);
	XAssert(state == State::PendingArrayScopeBegin);
	
	if (nestingStackSize >= 64)
	{
		errorCode = JSONErrorCode::MaximumNestingDepthReached;
		return false;
	}
	nestingStackBits.set(nestingStackSize);
	nestingStackSize++;

	const char leftSquareBracket = charsReader.get();
	XAssert(leftSquareBracket == '[');

	if (!skipWhitespaceAndComments())
		return false;

	state = charsReader.peek() == ']' ? State::PendingArrayScopeEnd : State::PendingValue;
	return true;
}

bool JSONReader::closeArray()
{
	XAssert(errorCode == JSONErrorCode::Success);
	XAssert(state == State::PendingArrayScopeEnd);

	XAssert(nestingStackSize > 0);
	XAssert(nestingStackBits.isSet(nestingStackSize - 1));
	nestingStackSize--;

	const char rightSquareBracket = charsReader.get();
	XAssert(rightSquareBracket == ']');

	return consumeCommaAndSkipToNextElementOrPrepareScopeClose();
}

bool JSONReader::isEndOfObject() const
{
	XAssert(errorCode == JSONErrorCode::Success);
	XAssert(nestingStackSize > 0 && nestingStackSize <= 64 && !nestingStackBits.isSet(nestingStackSize - 1));
	return state == State::PendingObjectScopeEnd;
}

bool JSONReader::isEndOfArray() const
{
	XAssert(errorCode == JSONErrorCode::Success);
	XAssert(nestingStackSize > 0 && nestingStackSize <= 64 && nestingStackBits.isSet(nestingStackSize - 1));
	return state == State::PendingArrayScopeEnd;
}

bool JSONReader::isEndOfDocument() const
{
	XAssert(errorCode == JSONErrorCode::Success);
	return state == State::EndOfDocumentReached;
}

bool JSONReader::isInsideObjectScope() const
{
	XAssert(errorCode == JSONErrorCode::Success);
	if (nestingStackSize == 0)
		return false;
	XAssert(nestingStackSize <= 64);
	return !nestingStackBits.isSet(nestingStackSize - 1);
}

bool JSONReader::isInsideArrayScope() const
{
	XAssert(errorCode == JSONErrorCode::Success);
	if (nestingStackSize == 0)
		return false;
	XAssert(nestingStackSize <= 64);
	return nestingStackBits.isSet(nestingStackSize - 1);
}

bool JSONReader::isInsideRootScope() const
{
	XAssert(errorCode == JSONErrorCode::Success);
	return nestingStackSize == 0;
}

const char* XLib::JSONErrorCodeToString(JSONErrorCode code)
{
	switch (code)
	{
		case JSONErrorCode::Success:						return "no error";
		case JSONErrorCode::UnexpectedEndOfFile:			return "unexpected end of file";
		case JSONErrorCode::InvalidCharacter:				return "invalid character";
		case JSONErrorCode::InvalidStringEscape:			return "invalid string escape";
		case JSONErrorCode::InvalidKeyFormat:				return "invalid key format";
		case JSONErrorCode::InvalidValueFormat:				return "invalid value format";
		case JSONErrorCode::ExpectedColon:					return "expected ':'";
		case JSONErrorCode::ExpectedCommaOrCurlyBracket:	return "expected ',' or '}'";
		case JSONErrorCode::ExpectedCommaOrSquareBracket:	return "expected ',' or ']'";
		case JSONErrorCode::MaximumNestingDepthReached:		return "maximum nesting depth reached";
		case JSONErrorCode::UnexpectedTextAfterRootElement:	return "unexpected text after root element";
		case JSONErrorCode::UnicodeSupportNotImplemented:	return "unicode support not implemented";
	}
	XAssertUnreachableCode();
	return "INVALID_ERROR_CODE";
}
