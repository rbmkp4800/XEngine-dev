#include "PseudoCTokenizer.h"

using namespace XLib;

inline auto PseudoCTokenizer::ComposeSimpleToken(TokenType type) -> Token
{
	Token token = {};
	token.type = type;
	return token;
}

void PseudoCTokenizer::reset(const char* data, uintptr length)
{
	textReader = MemoryTextReader(data, length);
}

auto PseudoCTokenizer::getToken() -> Token
{
	for (;;)
	{
		TextSkipWhitespaces(textReader);
		if (!textReader.canGetChar())
			return ComposeSimpleToken(TokenType::EndOfText);

		const char tokenFirstChar = textReader.peekCharUnsafe();

		if (tokenFirstChar == '\"')
		{
			textReader.getCharUnsafe();
			const char* stringLiteralBegin = textReader.getCurrentPtr();

			for (;;)
			{
				char c = char(textReader.getChar());
				if (c == '\0')
					return ComposeSimpleToken(TokenType::Error); // Unexpected end of text.
				if (c == '\r' || c == '\n')
					return ComposeSimpleToken(TokenType::Error); // Unexpected new line.
				if (c == '\\')
				{
					c = char(textReader.getChar());
					if (c == '\0')
						return ComposeSimpleToken(TokenType::Error); // Unexpected end of text.
				}
				if (c == '\"')
				{
					Token token = {};
					token.string = StringViewASCII(stringLiteralBegin, textReader.getCurrentPtr());
					token.type = TokenType::StringLiteral;
					return token;
				}
			}
		}

		if (IsLetter(tokenFirstChar) || tokenFirstChar == '_')
		{
			const char* identifierBegin = textReader.getCurrentPtr();
			textReader.getCharUnsafe();
			for (;;)
			{
				char c = char(textReader.peekChar());
				if (!IsLetterOrDigit(c) && c != '_')
				{
					Token token = {};
					token.string = StringView(identifierBegin, textReader.getCurrentPtr());
					token.type = TokenType::Identifier;
					return token;
				}
				else
					textReader.getCharUnsafe();
			}
		}

		sint64 numberLiteralValue = 0;
		if (TextReadInt(textReader, &numberLiteralValue))
		{
			Token token = {};
			token.number = numberLiteralValue;
			token.type = TokenType::NumberLiteral;
			return token;
		}

		switch (tokenFirstChar)
		{
			case '(':
			case ')':
			case '[':
			case ']':
			case '{':
			case '}':
			case ',':
			case '.':
			case ':':
			case ';':
			case '=':
				textReader.getCharUnsafe();
				return ComposeSimpleToken(TokenType(tokenFirstChar));
		}

		if (tokenFirstChar == '/')
		{
			textReader.getCharUnsafe();
			if (textReader.peekChar() == '/') // Comment
			{
				TextSkipToNewLine(textReader);
				continue;
			}
			// Fallthrough to "Invalid token" case.
		}

		return ComposeSimpleToken(TokenType::Error); // Invalid token.
	}
}
