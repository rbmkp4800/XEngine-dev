#pragma once

#include "XLib.h"
#include "XLib.Containers.ArrayList.h"
#include "XLib.NonCopyable.h"
#include "XLib.String.h"
#include "XLib.Text.h"

namespace XLib
{
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

	class JSONDocumentTree : public NonCopyable
	{
	private:
		struct Node;
		using NodesList = ArrayList<Node, uint32, false>;

	private:
		NodesList nodes;
		const char* sourceText = nullptr;
		uint64 sourceTextLength = 0;

	private:
		//static inline JSONValueType getNodeType(const Node& node);

	public:
		using NodeId = uint32;
		static constexpr NodeId RootNodeId = NodeId(0);
		static constexpr NodeId InvalidNodeId = NodeId(-1);

		using ObjectPropsIterator = uint32;
		static constexpr ObjectPropsIterator InvalidObjectPropsIterator = ObjectPropsIterator(0);

		struct ObjectProperty
		{
			StringView key;
			NodeId value;
		};

	public:
		JSONDocumentTree() = default;
		inline ~JSONDocumentTree() { clear(); }

		bool parse(const char* text, uint64 length = uint64(-1));
		inline void clear();

		inline bool isEmpty() const { return nodes.getSize() == 0; }
		inline bool isValidNode(NodeId id) const { return uint32(id) < nodes.getSize(); }
		inline JSONValueType getNodeType(NodeId nodeId) const;

		inline bool isObject(NodeId nodeId) const;
		inline bool isArray(NodeId nodeId) const;
		inline bool isString(NodeId nodeId) const;
		inline bool isNumber(NodeId nodeId) const;
		inline bool isBool(NodeId nodeId) const;
		inline bool isNull(NodeId nodeId) const;

		NodeId findObjectProperty(NodeId objectNodeId, const char* key) const;
		NodeId getByPointer(const char* pointer);

		inline ObjectPropsIterator getObjectPropsBegin(NodeId objectNodeId) const;
		inline ObjectPropsIterator getObjectPropsNext(ObjectPropsIterator objectIterator) const;
		inline ObjectProperty resolveObjectPropsIterator(ObjectPropsIterator objectIterator) const;

		inline NodeId getArrayBegin(NodeId arrayNodeId) const;
		inline NodeId getArrayNext(NodeId arrayElementNodeId) const;

		inline StringView getString(NodeId nodeId) const;
		inline uint64 getNumberAsI64(NodeId nodeId) const;
		inline float64 getNumberAsF64(NodeId nodeId) const;
		inline bool getBool(NodeId nodeId) const;
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	struct JSONDocumentTree::Node
	{
		uint typeBit; // 1 bit
		uint btreeLeftId;
		uint btreeRightId;
		uint keyStringOffset; // 34 bit
		uint keyStringLength; // 12 bit
		uint nextPropertyKeyId;
	};

	bool JSONDocumentTree::parse(const char* text, const uint64 length)
	{
		clear();

		sourceText = text;
		sourceTextLength = length;

		using Reader = FormatReader<TextStreamReader>;

		TextStreamReader baseReader;
		Reader fmtReader(baseReader);

		// returns false if syntax is incorrect (unexpected symbol)
		auto SkipWhitespacesAndComments = [](Reader& reader) -> bool
		{
			for (;;)
			{
				reader.skipWhitespaces();
				if (reader.peek() != '/')
					return true;

				reader.get();

				char c = reader.peek();
				if (c == '/')
				{
					// single line comment
					reader.skipToNewLine();
				}
				else if (c == '*')
				{
					// multiline comment 
					reader.get();
					for (;;)
					{
						reader.skipToChar('*');
						if (reader.endOfStream())
							return false;
						if (reader.get() == '/')
							break;
					}
				}
				else
					return false;
			}
		};

		for (;;)
		{
			if (!SkipWhitespacesAndComments(fmtReader))
			{

			}

			char c = baseReader.get();
			if (c == '{')
			{

			}
			else if (c == '[')
			{

			}
			else if (c == '\"')
			{

			}
			else if (c == '\0')
			{

			}
		}
	}

	inline void JSONDocumentTree::clear()
	{
		nodes.clear();
		sourceText = nullptr;
		sourceTextLength = 0;
	}
}
