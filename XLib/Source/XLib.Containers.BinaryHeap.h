#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.Containers.Vector.h"

namespace XLib
{
	namespace Internal
	{
		struct BinaryHeapComparatorMax abstract final
		{
			template <typename Type>
			static inline bool Compare(const Type& a, const Type& b) { return a > b; }
		};

		struct BinaryHeapComparatorMin abstract final
		{
			template <typename Type>
			static inline bool Compare(const Type& a, const Type& b) { return b > a; }
		};

		template <typename Type, typename Comparator>
		class BinaryHeapBase : public NonCopyable
		{
		private:
			Vector<Type> buffer;

			void heapify(uint32 index)
			{
				uint32 left = 2 * index + 1;
				uint32 right = 2 * index + 2;
				uint32 root = index;
				if (left < buffer.size() && Comparator::Compare(buffer[left], buffer[root]))
					root = left;
				if (right < buffer.size() && Comparator::Compare(buffer[right], buffer[root]))
					root = right;

				if (root != index)
				{
					swap(buffer[root], buffer[index]);
					heapify(root);
				}
			}

			inline void shiftUp(uint32 index)
			{
				while (index && Comparator::Compare(buffer[index], buffer[index / 2]))
				{
					swap(buffer[index / 2], buffer[index]);
					index /= 2;
				}
			}

		public:
			inline void insert(const Type& value)
			{
				uint32 index = buffer.size();
				buffer.pushBack(value);
				shiftUp(index);
			}
			inline Type remove()
			{
				Type value = buffer.front();
				buffer.front() = buffer.popBack();
				heapify(0);
				return value;
			}
			inline Type& peek() { return buffer.front(); }

			template <typename ShiftValueType>
			inline void shift(const ShiftValueType& value)
			{
				for each (Type& element in buffer)
					element += value;
			}

			template <typename KeyType>
			inline bool removeRandom(const KeyType& key)
			{
				for (uint32 i = 0; i < buffer.size(); i++)
				{
					if (buffer[i] == key)
					{
						buffer[i] = buffer.back();
						buffer.dropBack();
						shiftUp(i);
						return true;
					}
				}
				return false;
			}

			inline bool isEmpty() { return buffer.isEmpty(); }
		};
	}

	template <typename Type>
	class BinaryHeapMin : public NonCopyable
	{
	private:
		Internal::BinaryHeapBase<Type, Internal::BinaryHeapComparatorMin> base;

	public:
		inline void insert(const Type& value) { base.insert(value); }
		inline Type removeMin() { return base.remove(); }
		inline Type& peekMin() { return base.peek(); }

		template <typename ShiftValueType> void shift(const ShiftValueType& value) { base.shift(value); }
		template <typename KeyType> bool remove(const KeyType& key) { return base.removeRandom(key); }

		inline bool isEmpty() { return base.isEmpty(); }
	};

	template <typename Type>
	class BinaryHeapMax : public NonCopyable
	{
	private:
		Internal::BinaryHeapBase<Type, Internal::BinaryHeapComparatorMax> base;

	public:
		inline void insert(const Type& value) { base.insert(value); }
		inline Type removeMax() { return base.remove(); }
		inline Type& peekMax() { return base.peek(); }

		template <typename ShiftValueType> void shift(const ShiftValueType& value) { base.shift(value); }
		template <typename KeyType> bool remove(const KeyType& key) { return base.removeRandom(key); }

		inline bool isEmpty() { return base.isEmpty(); }
	};
}