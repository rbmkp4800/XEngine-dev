#include "PseudoCTokenizer.h"

using namespace XLib;

inline auto PseudoCTokenizer::ComposeSimpleToken(TokenType type, uint32 line, uint32 offset) -> Token
{
	Token token = {};
	token.line = line;
	token.offset = offset;
	token.type = type;
	return token;
}

void PseudoCTokenizer::reset(const char* data, uintptr length)
{
	textReader = MemoryTextReaderWithCursor(data, length);
}

auto PseudoCTokenizer::getToken() -> Token
{
	for (;;)
	{
		TextSkipWhitespaces(textReader);
		if (!textReader.canGetChar())
			return ComposeSimpleToken(TokenType::EndOfText, textReader.getCursorLine(), textReader.getCursorOffset());

		const char tokenFirstChar = textReader.peekCharUnsafe();
		const uint32 tokenLine = textReader.getCursorLine();
		const uint32 tokenOffset = textReader.getCursorOffset();

		if (tokenFirstChar == '\"')
		{
			textReader.getCharUnsafe();
			const char* stringLiteralBegin = textReader.getCurrentPtr();

			for (;;)
			{
				char c = char(textReader.getChar());
				if (c == '\0')
					return ComposeSimpleToken(TokenType::Error, tokenLine, tokenOffset); // Unexpected end of text.
				if (c == '\r' || c == '\n')
					return ComposeSimpleToken(TokenType::Error, tokenLine, tokenOffset); // Unexpected new line.
				if (c == '\\')
				{
					c = char(textReader.getChar());
					if (c == '\0')
						return ComposeSimpleToken(TokenType::Error, tokenLine, tokenOffset); // Unexpected end of text.
				}
				if (c == '\"')
				{
					Token token = {};
					token.string = StringViewASCII(stringLiteralBegin, textReader.getCurrentPtr() - 1);
					token.line = tokenLine;
					token.offset = tokenOffset;
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
					token.line = tokenLine;
					token.offset = tokenOffset;
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
			token.line = tokenLine;
			token.offset = tokenOffset;
			token.type = TokenType::NumberLiteral;
			return token;
		}

		if (tokenFirstChar == '/')
		{
			textReader.getCharUnsafe();
			if (textReader.peekChar() == '/') // Single-line comment.
			{
				TextSkipToNewLine(textReader);
				continue;
			}
			else if (textReader.peekChar() == '*') // Multi-line comment.
			{
				textReader.getCharUnsafe();

				// TODO: Rewrite this using `TextSkipToFirstOccurrence` when ready.
				for (;;)
				{
					if (textReader.getChar() == '*' && textReader.getChar() == '/')
						break;
					if (!textReader.canGetChar())
					{
						return ComposeSimpleToken(TokenType::EndOfText,
							textReader.getCursorLine(), textReader.getCursorOffset()); // Unexpected end of text in multi-line comment.
					}
				}
				continue;
			}
		}

		switch (tokenFirstChar)
		{
			// TODO: Implement all tokens.
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
			case '<':
			case '>':
				textReader.getCharUnsafe();
				return ComposeSimpleToken(TokenType(tokenFirstChar), tokenLine, tokenOffset);
		}

		return ComposeSimpleToken(TokenType::Error, tokenLine, tokenOffset); // Invalid token.
	}
}
