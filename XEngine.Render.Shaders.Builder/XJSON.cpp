#include "XJSON.h"

using namespace XLib;
using namespace XJSON;

static const char* ErrorMessageForUnexpectedEOF = "unexpected end-of-file";
static const char* ErrorMessageForUnexpectedEOFMultilineComment = "unexpected end-of-file in multiline comment";
static const char* ErrorMessageForUnexpectedEOFInStringLiteral = "unexpected end-of-file in string literal";
static const char* ErrorMessageForUnexpectedEOLInStringLiteral = "unexpected end-of-line in string literal";
static const char* ErrorMessageForExpectedCommaOrEndOfScope = "expected comma or end-of-scope";
static const char* ErrorMessageForExpectedColonCommaOrEndOfScope = "expected colon, comma or end-of-scope";
static const char* ErrorMessageForExpectedKeyLiteralOrEndOfScope = "expected key literal or end-of-scope";
static const char* ErrorMessageForExpectedTypeAnnotationLiteral = "expected type annotation literal";
static const char* ErrorMessageForExpectedRightParen = "expected ')'";
static const char* ErrorMessageForExpectedValueOrTypeAnnotation = "expected value or type annotation";

// TODO: Do something with this.
static bool IsUnquotedLiteralStartCharacter(char c) { return Char::IsLetterOrDigit(c) || c == '-' || c == '+' || c == '.' || c == '_' || c == '$'; }
static bool IsUnquotedLiteralPartCharacter(char c) { return Char::IsLetterOrDigit(c) || c == '-' || c == '+' || c == '.' || c == '_' || c == '<' || c == '>'; }

enum class MemoryStreamParser::ScopeType : uint8
{
	None = 0, // Root
	Object,
	Array,
};

enum class MemoryStreamParser::ExpectedOperator : uint8
{
	None = 0,
	ObjectScopeOpen,
	ArrayScopeOpen,
	ScopeClose,
	CommaOrScopeClose,
};

enum class MemoryStreamParser::ReadLiteralStatus : uint8
{
	Success = 0,
	Error,
	NotLiteral,
};

inline Location MemoryStreamParser::getCurrentLocation() const
{
	Location result = {};
	result.lineNumber = textReader.getLineNumber();
	result.columnNumber = textReader.getColumnNumber();
	result.absoluteOffset = textReader.getAbsoluteOffset();
	return result;
}

bool MemoryStreamParser::skipWhitespacesAndComments(ParserError& error)
{
	for (;;)
	{
		TextSkipWhitespaces(textReader);
		if (!textReader.canGetChar())
			return true;

		// Hacky but will do the thing.
		// I do not think that we need text reader with full rollback support just for this.
		if (textReader.getAvailableBytes() < 2)
			return true;

		const char* potentialCommentStartPtr = textReader.getCurrentPtr();
		const char c0 = potentialCommentStartPtr[0];
		const char c1 = potentialCommentStartPtr[1];

		const bool isSingleLineComment = c0 == '/' && c1 == '/';
		const bool isMultilineComment = c0 == '/' && c1 == '*';

		if (isSingleLineComment || isMultilineComment)
		{
			textReader.getChar();
			textReader.getChar();
			XAssert(textReader.getCurrentPtr() == potentialCommentStartPtr + 2); // Check that our hacks worked out.
		}

		if (isSingleLineComment)
		{
			for (;;)
			{
				if (!textReader.canGetChar())
					return true;
				const char c = textReader.getChar();
				if (c == '\n')
					break;
				if (c == '\\')
				{
					const char c2 = textReader.getChar();
					if (c2 == '\n')
						continue;
					if (c2 == '\r' && textReader.getChar() == '\n')
						continue;
				}
			}
		}
		else if (isMultilineComment)
		{
			// TODO: Use `TextSkipToFirstOccurrence` instead.

			for (;;)
			{
				if (textReader.getChar() == '*' && textReader.peekChar() == '/')
				{
					textReader.getChar();
					break;
				}
				if (!textReader.canGetChar())
				{
					error.message = ErrorMessageForUnexpectedEOFMultilineComment;
					error.location = getCurrentLocation();
					return false;
				}
			}
		}
		else
			return true;
	}
}

bool MemoryStreamParser::skipWhitespacesAndCommentsUnexpectedEOF(ParserError& error)
{
	if (!skipWhitespacesAndComments(error))
		return false;

	if (!textReader.canGetChar())
	{
		error.message = ErrorMessageForUnexpectedEOF;
		error.location = getCurrentLocation();
		return false;
	}

	return true;
}

auto MemoryStreamParser::readLiteral(StringViewASCII& resultLiteral, ParserError& error) -> ReadLiteralStatus
{
	const char* literalStartPtr = textReader.getCurrentPtr();
	const char literalStartChar = textReader.peekChar();

	if (literalStartChar == '"')
	{
		textReader.getChar();

		enum class EscapeState : uint8
		{
			Normal = 0,
			BackslashConsumed,
			CRConsumed,
		};
		EscapeState escapeState = EscapeState::Normal;

		for (;;)
		{
			if (!textReader.canGetChar())
			{
				error.message = ErrorMessageForUnexpectedEOFInStringLiteral;
				error.location = getCurrentLocation();
				return ReadLiteralStatus::Error;
			}

			const char c = textReader.peekChar();

			if (c == '\"')
			{
				if (escapeState == EscapeState::BackslashConsumed)
					escapeState = EscapeState::Normal;
				else
					break;
			}
			else if (c == '\\')
			{
				if (escapeState == EscapeState::BackslashConsumed)
					escapeState = EscapeState::Normal;
				else
					escapeState = EscapeState::BackslashConsumed;
			}
			else if (c == '\n')
			{
				if (escapeState == EscapeState::BackslashConsumed || escapeState == EscapeState::CRConsumed)
					escapeState = EscapeState::Normal;
				else
				{
					error.message = ErrorMessageForUnexpectedEOLInStringLiteral;
					error.location = getCurrentLocation();
					return ReadLiteralStatus::Error;
				}
			}
			else if (c == '\r')
			{
				if (escapeState == EscapeState::BackslashConsumed)
					escapeState = EscapeState::CRConsumed;
			}
			else
			{
				if (escapeState == EscapeState::BackslashConsumed || escapeState == EscapeState::CRConsumed)
					escapeState = EscapeState::Normal;
			}

			textReader.getChar();
		}

		resultLiteral = StringViewASCII(literalStartPtr + 1, textReader.getCurrentPtr());

		const char closingQuote = textReader.getChar();
		XAssert(closingQuote == '\"');

		return ReadLiteralStatus::Success;
	}
	else if (IsUnquotedLiteralStartCharacter(literalStartChar))
	{
		textReader.getChar();

		for (;;)
		{
			if (!textReader.canGetChar())
				break;
			if (!IsUnquotedLiteralPartCharacter(textReader.peekChar()))
				break;
			textReader.getChar();
		}

		resultLiteral = StringViewASCII(literalStartPtr, textReader.getCurrentPtr());
		return ReadLiteralStatus::Success;
	}

	return ReadLiteralStatus::NotLiteral;
}

bool MemoryStreamParser::readValueInner(Value& resultValue, ParserError& error)
{
	XAssert(textReader.canGetChar());

	bool typeAnnotationConsumed = false;
	Location locationAfterTypeAnnotation = {};

	if (textReader.peekChar() == '(')
	{
		// XJSON extension: Type annotation.

		textReader.getChar();

		if (!skipWhitespacesAndCommentsUnexpectedEOF(error))
			return false;

		const Location typeAnnotationLiteralStartLocation = getCurrentLocation();
		const ReadLiteralStatus readTypeAnnotationLiteralStatus = readLiteral(resultValue.typeAnnotation, error);

		if (readTypeAnnotationLiteralStatus == ReadLiteralStatus::Error)
			return false;
		if (readTypeAnnotationLiteralStatus == ReadLiteralStatus::NotLiteral)
		{
			error.message = ErrorMessageForExpectedTypeAnnotationLiteral;
			error.location = typeAnnotationLiteralStartLocation;
			return false;
		}

		resultValue.typeAnnotationLocation = typeAnnotationLiteralStartLocation;

		if (!skipWhitespacesAndCommentsUnexpectedEOF(error))
			return false;

		if (textReader.getChar() != ')')
		{
			error.message = ErrorMessageForExpectedRightParen;
			error.location = getCurrentLocation();
			return false;
		}

		typeAnnotationConsumed = true;
		locationAfterTypeAnnotation = getCurrentLocation();
	}
	else
	{
		resultValue.typeAnnotation = StringViewASCII();
		resultValue.typeAnnotationLocation = {};
	}

	if (!skipWhitespacesAndCommentsUnexpectedEOF(error))
		return false;

	const Location valueStartLocation = getCurrentLocation();
	const char valueStartChar = textReader.peekChar();

	if (valueStartChar == '{')
	{
		resultValue.valueLiteral = StringViewASCII();
		resultValue.valueLocation = valueStartLocation;
		resultValue.valueType = ValueType::Object;
		expectedOperator = ExpectedOperator::ObjectScopeOpen;
		return true;
	}

	if (valueStartChar == '[')
	{
		resultValue.valueLiteral = StringViewASCII();
		resultValue.valueLocation = valueStartLocation;
		resultValue.valueType = ValueType::Array;
		expectedOperator = ExpectedOperator::ArrayScopeOpen;
		return true;
	}

	const ReadLiteralStatus readValueLiteralStatus = readLiteral(resultValue.valueLiteral, error);
	if (readValueLiteralStatus == ReadLiteralStatus::Error)
		return false;
	if (readValueLiteralStatus == ReadLiteralStatus::Success)
	{
		resultValue.valueLiteral; // Already filled in by `readLiteral()`.
		resultValue.valueLocation = valueStartLocation;
		resultValue.valueType = ValueType::Literal;
		expectedOperator = ExpectedOperator::CommaOrScopeClose;
		return true;
	}
	XAssert(readValueLiteralStatus == ReadLiteralStatus::NotLiteral);

	// If we are here, actual value was not consumed.
	// But if at least type annotation was consumed, then it is ok. XJSON allows it.
	if (typeAnnotationConsumed)
	{
		resultValue.valueLiteral = StringViewASCII();
		resultValue.valueLocation = locationAfterTypeAnnotation;
		resultValue.valueType = ValueType::None;
		expectedOperator = ExpectedOperator::CommaOrScopeClose;
		return true;
	}
	
	// No type annotation was consumed. It is an error.
	error.message = ErrorMessageForExpectedValueOrTypeAnnotation;
	error.location = valueStartLocation;
	return false;
}

void MemoryStreamParser::initialize(const char* input, uintptr inputLength)
{
	textReader.initialize(input, inputLength);
}

void MemoryStreamParser::forceOpenRootValueScopeAsObject()
{
	XAssert(currentScopeType == ScopeType::None);
	XAssert(stack.isEmpty());

	currentScopeType = ScopeType::Object;
}

void MemoryStreamParser::forceOpenRootValueScopeAsArray()
{
	XAssert(currentScopeType == ScopeType::None);
	XAssert(stack.isEmpty());

	currentScopeType = ScopeType::Array;
}

ReadStatus MemoryStreamParser::readValue(Value& resultRootValue, ParserError& error)
{
	XAssert(expectedOperator == ExpectedOperator::None || expectedOperator == ExpectedOperator::CommaOrScopeClose);

	if (currentScopeType == ScopeType::None)
		XAssert(stack.isEmpty());	// This is document root.
	else
		XAssert(currentScopeType == ScopeType::Array);

	const bool insideRootScopeThatWasForceOpenedAsArray = stack.isEmpty() && currentScopeType == ScopeType::Array;

	// Consume comma.
	if (expectedOperator == ExpectedOperator::CommaOrScopeClose)
	{
		if (!skipWhitespacesAndComments(error))
			return ReadStatus::Error;
		if (textReader.peekChar() == ',')
		{
			textReader.getChar();
			expectedOperator = ExpectedOperator::None;
		}
	}

	// Read value.
	// First check for end of scope.

	if (!skipWhitespacesAndComments(error))
		return ReadStatus::Error;

	bool isEndOfScope = false;
	if (textReader.canGetChar())
	{
		const bool explicitEndOfScopeExpected = !insideRootScopeThatWasForceOpenedAsArray;
		isEndOfScope = textReader.peekChar() == ']' && explicitEndOfScopeExpected;
	}
	else
	{
		isEndOfScope = insideRootScopeThatWasForceOpenedAsArray;

		if (!isEndOfScope)
		{
			error.message = ErrorMessageForUnexpectedEOF;
			error.location = getCurrentLocation();
			return ReadStatus::Error;
		}
	}

	if (isEndOfScope)
	{
		expectedOperator = ExpectedOperator::ScopeClose;
		return ReadStatus::EndOfScope;
	}

	// Scope is not closed. Comma should have been consumed.
	if (expectedOperator == ExpectedOperator::CommaOrScopeClose)
	{
		error.message = ErrorMessageForExpectedCommaOrEndOfScope;
		error.location = getCurrentLocation();
		return ReadStatus::Error;
	}

	if (!readValueInner(resultRootValue, error))
		return ReadStatus::Error;

	return ReadStatus::Success;
}

ReadStatus MemoryStreamParser::readKeyValue(KeyValue& resultKeyValue, ParserError& error)
{
	XAssert(expectedOperator == ExpectedOperator::None || expectedOperator == ExpectedOperator::CommaOrScopeClose);
	XAssert(currentScopeType == ScopeType::Object);

	const bool insideRootScopeThatWasForceOpenedAsObject = stack.isEmpty();

	// Consume comma.
	if (expectedOperator == ExpectedOperator::CommaOrScopeClose)
	{
		if (!skipWhitespacesAndComments(error))
			return ReadStatus::Error;
		if (textReader.peekChar() == ',')
		{
			textReader.getChar();
			expectedOperator = ExpectedOperator::None;
		}
	}

	// Read key literal.
	// First check for end of scope.

	if (!skipWhitespacesAndComments(error))
		return ReadStatus::Error;

	bool isEndOfScope = false;
	if (textReader.canGetChar())
	{
		const bool explicitEndOfScopeExpected = !insideRootScopeThatWasForceOpenedAsObject;
		isEndOfScope = textReader.peekChar() == '}' && explicitEndOfScopeExpected;
	}
	else
	{
		isEndOfScope = insideRootScopeThatWasForceOpenedAsObject;

		if (!isEndOfScope)
		{
			error.message = ErrorMessageForUnexpectedEOF;
			error.location = getCurrentLocation();
			return ReadStatus::Error;
		}
	}

	if (isEndOfScope)
	{
		expectedOperator = ExpectedOperator::ScopeClose;
		return ReadStatus::EndOfScope;
	}

	// Scope is not closed. Comma should have been consumed.
	if (expectedOperator == ExpectedOperator::CommaOrScopeClose)
	{
		error.message = ErrorMessageForExpectedCommaOrEndOfScope;
		error.location = getCurrentLocation();
		return ReadStatus::Error;
	}

	const Location keyLiteralStartLocation = getCurrentLocation();
	const ReadLiteralStatus readKeyLiteralStatus = readLiteral(resultKeyValue.key, error);

	if (readKeyLiteralStatus == ReadLiteralStatus::Error)
		return ReadStatus::Error;
	if (readKeyLiteralStatus == ReadLiteralStatus::NotLiteral)
	{
		error.message = ErrorMessageForExpectedKeyLiteralOrEndOfScope;
		error.location = keyLiteralStartLocation;
		return ReadStatus::Error;
	}

	resultKeyValue.keyLocation = keyLiteralStartLocation;

	// Detect if key has corresponding value.

	if (!skipWhitespacesAndComments(error))
		return ReadStatus::Error;

	bool isKeyOnly = false;

	if (textReader.canGetChar())
	{
		const bool explicitEndOfScopeExpected = !insideRootScopeThatWasForceOpenedAsObject;
		isKeyOnly |= textReader.peekCharUnsafe() == '}' && explicitEndOfScopeExpected;
		isKeyOnly |= textReader.peekCharUnsafe() == ',';
	}
	else
	{
		if (insideRootScopeThatWasForceOpenedAsObject)
		{
			isKeyOnly = true;
		}
		else
		{
			error.message = ErrorMessageForUnexpectedEOF;
			error.location = getCurrentLocation();
			return ReadStatus::Error;
		}
	}

	if (isKeyOnly)
	{
		// XJSON extension: Key only.

		resultKeyValue.value.typeAnnotation = StringViewASCII();
		resultKeyValue.value.typeAnnotationLocation = {};
		resultKeyValue.value.valueLiteral = StringViewASCII();
		resultKeyValue.value.valueLocation = resultKeyValue.keyLocation;
		resultKeyValue.value.valueType = ValueType::None;
		expectedOperator = ExpectedOperator::CommaOrScopeClose;
		return ReadStatus::Success;
	}

	if (textReader.peekChar() != ':')
	{
		error.message = ErrorMessageForExpectedColonCommaOrEndOfScope;
		error.location = getCurrentLocation();
		return ReadStatus::Error;
	}
	textReader.getChar(); // Skip ':'.

	// Read value.

	if (!skipWhitespacesAndCommentsUnexpectedEOF(error))
		return ReadStatus::Error;

	if (!readValueInner(resultKeyValue.value, error))
		return ReadStatus::Error;

	return ReadStatus::Success;
}

void MemoryStreamParser::openObjectValueScope()
{
	XAssert(expectedOperator == ExpectedOperator::ObjectScopeOpen);
	XAssert(!stack.isFull());
	stack.pushBack(StackElement { currentScopeType });

	currentScopeType = ScopeType::Object;
	expectedOperator = ExpectedOperator::None;

	const char c = textReader.getChar();
	XAssert(c == '{');
}

void MemoryStreamParser::closeObjectValueScope()
{
	XAssert(currentScopeType == ScopeType::Object);
	XAssert(expectedOperator == ExpectedOperator::ScopeClose);
	XAssert(!stack.isEmpty());
	const StackElement stackBack = stack.popBack();

	currentScopeType = stackBack.scopeType;
	expectedOperator = ExpectedOperator::CommaOrScopeClose;

	const char c = textReader.getChar();
	XAssert(c == '}');
}

void MemoryStreamParser::openArrayValueScope()
{
	XAssert(expectedOperator == ExpectedOperator::ArrayScopeOpen);
	XAssert(!stack.isFull());
	stack.pushBack(StackElement { currentScopeType });

	currentScopeType = ScopeType::Array;
	expectedOperator = ExpectedOperator::None;

	const char c = textReader.getChar();
	XAssert(c == '[');
}

void MemoryStreamParser::closeArrayValueScope()
{
	XAssert(currentScopeType == ScopeType::Array);
	XAssert(expectedOperator == ExpectedOperator::ScopeClose);
	XAssert(!stack.isEmpty());
	const StackElement stackBack = stack.popBack();

	currentScopeType = stackBack.scopeType;
	expectedOperator = ExpectedOperator::CommaOrScopeClose;

	const char c = textReader.getChar();
	XAssert(c == ']');
}
