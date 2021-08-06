#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.Heap.h"

namespace XLib
{
	template <typename Key, typename Value>
	class FlatHashMap : public NonCopyable
	{
	private:
		struct Entry
		{
			Key key;
			Value value;
		};

	public:
		FlatHashMap() = default;
		~FlatHashMap() = default;


	};
}