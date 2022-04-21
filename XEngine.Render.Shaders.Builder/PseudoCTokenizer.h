#pragma once

#include <XLib.h>
#include <XLib.String.h>
#include <XLib.Text.h>

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

		uint32 lineIndex;
		uint16 charIndex;

		TokenType type;
	};

private:
	XLib::MemoryTextReader textReader;

private:
	static inline Token ComposeSimpleToken(TokenType type);

public:
	PseudoCTokenizer() = default;
	~PseudoCTokenizer() = default;

	void reset(const char* data, uintptr length);

	Token getToken();
};
