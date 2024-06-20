#pragma once

#include "XLib.h"
#include "XLib.CharStream.h"
#include "XLib.Containers.BitArray.h"
#include "XLib.NonCopyable.h"
#include "XLib.String.h"

namespace XLib
{
	enum class JSONErrorCode : uint8
	{
		Success = 0,
		UnexpectedEndOfFile,
		InvalidCharacter,
		InvalidStringEscape,
		InvalidKeyFormat,
		InvalidValueFormat,
		ExpectedColon,
		ExpectedCommaOrCurlyBracket,
		ExpectedCommaOrSquareBracket,
		MaximumNestingDepthReached,
		UnexpectedTextAfterRootElement,
		UnicodeSupportNotImplemented,
	};

	enum class JSONValueType : uint8
	{
		Undefined = 0,
		Object,
		Array,
		String,
		Number,
		Boolean,
		Null,
	};

	struct JSONString
	{
		StringViewASCII string;
		uintptr unescapedLength;
		bool isEscaped;

		void unescape(char* result);
	};

	struct JSONNumber
	{
		union
		{
			float64 floatingPoint;
			sint64 integet;
		};
	};

	struct JSONValue
	{
		union
		{
			JSONString string;
			JSONNumber number;
			bool boolean;
		};

		JSONValueType type;
	};

	class JSONReader : public NonCopyable
	{
	private:
		enum class State : uint8;

		class CharsReader : public XLib::LineColumnTrackingCharStreamReaderWrapper<XLib::MemoryCharStreamReader>
		{
		private:
			using Base = XLib::LineColumnTrackingCharStreamReaderWrapper<XLib::MemoryCharStreamReader>;

		private:
			XLib::MemoryCharStreamReader innerMemoryReader;

		public:
			inline CharsReader() : Base(innerMemoryReader) {}
			inline void open(const char* data, uintptr length) { innerMemoryReader.open(data, length); }
			inline const char* getCurrentPtr() const { return innerMemoryReader.getCurrentPtr(); }
		};

	private:
		CharsReader charsReader;
		InplaceBitArray<64> nestingStackBits; // Each bit is one nested scope. 0 - object, 1 - array.
		uint8 nestingStackSize = 0;
		State state = State(0);
		JSONErrorCode errorCode = JSONErrorCode::Success;

	private:
		bool tryConsumeChar(char c);
		bool skipWhitespaceAndComments();

		bool parseString(JSONString& result);			// Caller side should guarantee that next char is '\"'.
		bool parseIdentifier(StringViewASCII& result);	// Caller side should guarantee that next char is valid identifier start char.
		bool parseNumber(JSONNumber& result);			// Caller side should guarantee that next char is valid number start char.

		bool parseKey(JSONString& result);
		bool parseLiteralValue(JSONValue& result);

		bool consumeCommaAndSkipToNextElementOrPrepareScopeClose(); // Makes state trainsition.

	public:
		JSONReader() = default;
		~JSONReader() = default;

		bool openDocument(const char* text, uintptr length);

		bool readKey(JSONString& result);
		bool readValue(JSONValue& result);

		bool openObject();
		bool closeObject();

		bool openArray();
		bool closeArray();

		bool isEndOfObject() const;
		bool isEndOfArray() const;
		bool isEndOfDocument() const;

		bool isInsideObjectScope() const;
		bool isInsideArrayScope() const;
		bool isInsideRootScope() const;

		inline JSONErrorCode getErrorCode() const { return errorCode; }
		inline uint32 getLineNumer() const { return charsReader.getLineNumber(); }
		inline uint32 getColumnNumer() const { return charsReader.getColumnNumber(); }
		//inline uintptr getOffset() const { return charsReader.getOffset(); }
	};

	const char* JSONErrorCodeToString(JSONErrorCode code);
}
