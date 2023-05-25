#pragma once

#include <XLib.h>
#include <XLib.Containers.ArrayList.h>
#include <XLib.String.h>
#include <XLib.Text.h>

// TODO: Write tests for all cases.
// TODO: When closing root scope, we should parse file to EOF and check there is nothing after root scope.
// TODO: When error occurs, fail all subsequent calls.

// TODO: Copilot dreamed it. Take a look later. Probably can save few lines of boilerplate.
//	xjsonProperty.value.arrayValue->forEach([&](const XJSON::Value& xjsonValue)
//	{
//		...
//	});

namespace XJSON
{
	enum class ReadStatus : uint8
	{
		Success = 0,
		EndOfScope,
		Error,
	};

	struct Location
	{
		uint32 lineNumber;
		uint32 columnNumber;
		uintptr absoluteOffset;
	};

	struct ParserError
	{
		const char* message; // This is guaranteed to be pointer to static string
		Location location;
	};

	enum class ValueType : uint8
	{
		None = 0,
		Object,
		Array,
		Literal,
	};

	struct Value
	{
		XLib::StringViewASCII typeAnnotation;
		Location typeAnnotationLocation;

		XLib::StringViewASCII valueLiteral;
		Location valueLocation;

		ValueType valueType;
	};

	struct KeyValue
	{
		XLib::StringViewASCII key;
		Location keyLocation;

		Value value;
	};

	class MemoryStreamParser : public XLib::NonCopyable
	{
	private:
		static constexpr uint32 MaxDepth = 64;

		enum class ScopeType : uint8;
		enum class ExpectedOperator : uint8;
		enum class ReadLiteralStatus : uint8;

		struct StackElement
		{
			ScopeType scopeType;
		};

	private:
		XLib::MemoryTextReaderWithLocation textReader;
		XLib::InplaceArrayList<StackElement, MaxDepth> stack;
		ScopeType currentScopeType = ScopeType(0);
		ExpectedOperator expectedOperator = ExpectedOperator(0);

	private:
		inline Location getCurrentLocation() const;

		bool skipWhitespacesAndComments(ParserError& error); // Returns false on error (EOF in multiline comment).
		bool skipWhitespacesAndCommentsUnexpectedEOF(ParserError& error); // Guaranteed no EOF if returned true.
		ReadLiteralStatus readLiteral(XLib::StringViewASCII& resultLiteral, ParserError& error);
		bool readValueInner(Value& resultValue, ParserError& error); // Returns false on error. End-of-scope should be handled externally.

	public:
		MemoryStreamParser() = default;
		~MemoryStreamParser() = default;

		void initialize(const char* input, uintptr inputLength);
		void destroy();

		void forceOpenRootValueScopeAsObject();
		void forceOpenRootValueScopeAsArray();

		ReadStatus readValue(Value& resultValue, ParserError& error);
		ReadStatus readKeyValue(KeyValue& resultKeyValue, ParserError& error);

		void openObjectValueScope();
		void closeObjectValueScope();

		void openArrayValueScope();
		void closeArrayValueScope();
	};
}
