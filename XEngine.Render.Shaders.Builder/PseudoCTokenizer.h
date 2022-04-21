#pragma once

#include <XLib.h>
#include <XLib.String.h>
#include <XLib.Text.h>

// TODO: Do this properly.
class MemoryTextReaderWithCursor : public XLib::MemoryTextReader
{
private:
	using Base = XLib::MemoryTextReader;

private:
	uint32 cursorLine = 0;
	uint32 cursorOffset = 0;
	bool crConsumed = false;

private:
	inline uint32 advanceCursor(uint32 c)
	{
		// TODO: Handle end of text properly.

		if (c == '\r')
		{
			cursorLine++;
			cursorOffset = 0;
			crConsumed = true;
		}
		else if (c == '\n')
		{
			cursorLine += !crConsumed;
			cursorOffset = 0;
			crConsumed = false;
		}
		else
		{
			cursorOffset++;
			crConsumed = false;
		}
		return c;
	}

public:
	MemoryTextReaderWithCursor() = default;
	~MemoryTextReaderWithCursor() = default;

	inline MemoryTextReaderWithCursor(const char* data, uintptr length = uintptr(-1)) : Base(data, length) {}

	inline uint32 getChar() { return advanceCursor(Base::getChar()); }
	inline char getCharUnsafe() { return (char)advanceCursor(Base::getCharUnsafe()); }
	//inline void readChars();

	inline uint32 getCursorLine() const { return cursorLine; }
	inline uint32 getCursorOffset() const { return cursorOffset; }
};

class PseudoCTokenizer
{
public:
	enum class TokenType : uint8
	{
		EndOfText = 0,
		Error,
		Identifier,
		NumberLiteral,
		StringLiteral,

		LeftParenthesis = '(',
		RightParenthesis = ')',
		LeftBracket = '[',
		RightBracket = ']',
		LeftBrace = '{',
		RightBrace = '}',
		Comma = ',',
		Dot = '.',
		Colon = ':',
		Semicolon = ';',
		Equal = '=',
	};

	struct Token
	{
		union
		{
			XLib::StringViewASCII string;
			sint64 number;
			// Error error;
		};

		uint32 line;
		uint32 offset;

		TokenType type;
	};

private:
	MemoryTextReaderWithCursor textReader;

private:
	static inline Token ComposeSimpleToken(TokenType type, uint32 line, uint32 offset);

public:
	PseudoCTokenizer() = default;
	~PseudoCTokenizer() = default;

	void reset(const char* data, uintptr length);

	Token getToken();
};
