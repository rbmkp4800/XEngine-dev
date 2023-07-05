#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"

namespace XLib
{
	template <typename KeyType, typename ValueType>
	class FlatHashMap : public NonCopyable
	{
	private:
		struct Entry
		{
			KeyType key;
			ValueType value;
		};

	public:
		FlatHashMap() = default;
		~FlatHashMap() = default;

		inline bool insert(const KeyType& key, const ValueType& value);
		inline ValueType* find(const KeyType& key);
	};
}
