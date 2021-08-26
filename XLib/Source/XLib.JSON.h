#pragma once

#include "XLib.h"
//#include "XLib.Containers.ArrayList.h"

namespace XLib
{
	class JSONStreamParser;

	enum class JSONValueType : uint8
	{
		None = 0,
		Object,
		Array,
		String,
		Number,
		Bool,
		Null,
	};

	class JSONStreamParserHandlerBase abstract
	{
		friend JSONStreamParser;

	protected:
		JSONStreamParserHandlerBase() = default;
		~JSONStreamParserHandlerBase() = default;

		virtual bool onKeyString(const char* str, uint32 length, bool truncated) = 0;

		virtual bool onValueObjectBegin() = 0;
		virtual bool onValueObjectEnd() = 0;
		virtual bool onValueArrayBegin() = 0;
		virtual bool onValueArrayEnd() = 0;

		virtual bool onValueString(const char* str, uint32 length, bool truncated) = 0;
		virtual bool onValueNumber(float64 value) = 0;
		virtual bool onValueBool(bool value) = 0;
		virtual bool onValueNull() = 0;

		//virtual bool onStringContinuation(const char* str, uint32 length, bool complete) = 0;
	};

	class JSONStreamParser
	{
	private:
		static constexpr uint32 InternalBufferSize = 0x100;

		enum class State : uint8;

		//enum class HierarchyStackItem : uint8;
		//using HierarchyStack = InplaceArrayList<HierarchyStackItem, 63, uint8>;

	private:
		char buffer[InternalBufferSize];
		JSONStreamParserHandlerBase& handler;

		State state = State(0);

	public:
		inline JSONStreamParser(JSONStreamParserHandlerBase& handler) : handler(handler) {}
		~JSONStreamParser() = default;

		void processPortion(const void* data, uint32 length);
	};
}
