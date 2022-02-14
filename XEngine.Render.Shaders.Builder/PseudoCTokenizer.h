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

		Keyword_PipelineLayout = 128,
		Keyword_GraphicsPipeline,
		Keyword_ComputePipeline,

		Keyword_ReadOnlyBuffer,
		Keyword_ReadWriteBuffer,
		Keyword_ReadOnlyTexture,
		Keyword_ReadWriteTexture,

		Keyword_SetLayout,
		Keyword_SetCS,
		Keyword_SetVS,
		Keyword_SetAS,
		Keyword_SetMS,
		Keyword_SetPS,
		Keyword_SetRT,
		Keyword_SetDepthRT,
	};

	struct Token
	{
		union
		{
			XLib::StringView string;
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

	inline PseudoCTokenizer(const char* data, uintptr length = uintptr(-1)) : textReader(data, length) {}

	Token getToken();
};
