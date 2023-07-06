#pragma once

#include <XLib.h>
#include <XLib.NonCopyable.h>
#include <XLib.String.h>
#include <XLib.Text.h>

#include <XEngine.Render.HAL.Common.h>

#include "XEngine.Render.HAL.ShaderCompiler.h"

// TODO: Add '#line' directives support.
// TODO: Lexer should not allow any calls after error.

namespace XEngine::Render::HAL::ShaderCompiler
{
	class HLSLPatcher : public XLib::NonCopyable
	{
	public:
		struct Location
		{
			uint32 lineNumber;
			uint32 columnNumber;
		};

		struct Error
		{
			XLib::InplaceStringASCIIx256 message;
			Location location;
		};

	private:
		enum class LexemeType : uint8;
		enum class AttributeType : uint8;
		enum class ResourceType : uint8;
		enum class BindingType : uint8;

		struct Lexeme
		{
			XLib::StringViewASCII string;
			Location location;
			LexemeType type;
		};

		struct Attribute;
		struct BindingInfo;

		class Lexer
		{
		private:
			XLib::MemoryTextReaderWithLocation textReader;
			Lexeme currentLexeme = {};

		public:
			inline Lexer(XLib::StringViewASCII sourceText) : textReader(sourceText.getData(), sourceText.getLength()) {}
			~Lexer() = default;

			bool advance(Error& error);

			inline Lexeme peekLexeme() const { return currentLexeme; }
			inline bool hasLexeme() const { return currentLexeme.type != LexemeType(0); }
			inline const char* getBeginPtr() const { return textReader.getBeginPtr(); }
			inline const char* getEndPtr() const { return textReader.getEndPtr(); }
		};

		// Composes output via ranges of input data (via lexer) or user provided data.
		// TODO: Maintains consistent line/column location with input, via whitespaces and #line directives.
		class OutputComposer
		{
		private:
			Lexer& inputLexer;
			const char* currentRangeStart = nullptr;
			XLib::DynamicStringASCII output;

		public:
			inline OutputComposer(Lexer& inputLexer) : inputLexer(inputLexer), currentRangeStart(inputLexer.getBeginPtr()) {}
			~OutputComposer() = default;

			void write(const XLib::StringViewASCII& text);

			void copyInputRangeUpToCurrentPosition();
			void blankOutInputRangeUpToCurrentPosition();

			XTODO("Check that this move is actually working")
			inline XLib::DynamicStringASCII composeOuput() { return asRValue(output); }
		};

	private:
		Lexer lexer;
		OutputComposer composer;
		const PipelineLayout& pipelineLayout;

	private:
		bool processAttribute(Attribute& attribute, Error& error);
		bool processVariableDefinitionForBinding(const BindingInfo& bindingInfo, Error& error);

		static bool ExtractBindingInfo(const PipelineLayout& pipelineLayout,
			XLib::StringViewASCII bindingRootName, Location bindingRootNameLocation,
			XLib::StringViewASCII bindingNestedName, Location bindingNestedNameLocation,
			BindingInfo& resultBindingInfo, Error& error);

	public:
		HLSLPatcher(XLib::StringViewASCII sourceText, const PipelineLayout& pipelineLayout);
		~HLSLPatcher() = default;

		bool execute(XLib::DynamicStringASCII& result, Error& error);
	};
}
