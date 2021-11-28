#pragma once

#include "XLib.h"

#define XLib_hookPrev(item)	( (Item*&) ( ((item)->*hook).prev ) )
#define XLib_hookNext(item)	( (Item*&) ( ((item)->*hook).next ) )

namespace XLib
{
	class IntrusiveDoublyLinkedListItemHook final
	{
		template <typename Item, IntrusiveDoublyLinkedListItemHook (Item::*hook)>
		friend class IntrusiveDoublyLinkedList;

	private:
		void *prev = nullptr;
		void *next = nullptr;

	public:
		IntrusiveDoublyLinkedListItemHook() = default;
		~IntrusiveDoublyLinkedListItemHook() = default;

		inline bool isHooked() const { return prev != nullptr || next != nullptr; }
	};

	template <typename Item, IntrusiveDoublyLinkedListItemHook (Item::*hook)>
	class IntrusiveDoublyLinkedList
	{
	private:
		Item *head = nullptr;
		Item *tail = nullptr;

	public:
		class Iterator final
		{
			friend class IntrusiveDoublyLinkedList;

		private:
			Item *i;

		private:
			inline Iterator(Item* i = nullptr) : i(i) {}

		public:
			inline Item& get() { return *i; }
			inline const Item& get() const { return *i; }
			inline void next() { i = XLib_hookNext(i); }
			inline void prev() { i = XLib_hookPrev(i); }
			inline bool isValid() const { return i != nullptr; }

			inline Item& operator * () { return *i; }
			inline const Item& operator * () const { return *i; }
			inline bool operator == (const Iterator& that) { return i == that.i; }
			inline bool operator != (const Iterator& that) { return i != that.i; }
			inline void operator ++ () { i = XLib_hookNext(i); }
			inline void operator -- () { i = XLib_hookPrev(i); }
		};

		class ReverseIterator;

	public:
		IntrusiveDoublyLinkedList() = default;
		~IntrusiveDoublyLinkedList() = default;

		inline void pushBack(Item* item)
		{
			XAssert(!XLib_hookPrev(item) && !XLib_hookNext(item)); // Item already in list

			if (tail)
			{
				XLib_hookNext(tail) = item;
				XLib_hookPrev(item) = tail;
				XLib_hookNext(item) = nullptr;
				tail = item;
			}
			else
			{
				XAssert(!head); // Internal structure broken

				XLib_hookPrev(item) = nullptr;
				XLib_hookNext(item) = nullptr;

				head = item;
				tail = item;
			}
		}
		inline Item* popFront()
		{
			XAssert(!isEmpty());

			Item* result = head;
			head = XLib_hookNext(result);
			if (!head)
				tail = nullptr;
			else
				XLib_hookPrev(head) = nullptr;

			XLib_hookPrev(result) = nullptr;
			XLib_hookNext(result) = nullptr;

			return result;
		}
		inline void remove(Item* item)
		{
			XAssert(XLib_hookPrev(item) || XLib_hookNext(item)); // Item not in list

			if (XLib_hookPrev(item))
				XLib_hookNext(XLib_hookPrev(item)) = XLib_hookNext(item);
			else
			{
				XAssert(head == item); // Item from another list
				head = XLib_hookNext(item);
			}

			if (XLib_hookNext(item))
				XLib_hookPrev(XLib_hookNext(item)) = XLib_hookPrev(item);
			else
			{
				XAssert(tail == item); // Item from another list
				tail = XLib_hookPrev(item);
			}

			XLib_hookPrev(item) = nullptr;
			XLib_hookNext(item) = nullptr;
		}

		inline void insert(Item* item) { pushBack(item); }

		inline void pushBack(Item& item) { pushBack(&item); }
		inline void insert(Item& item) { insert(&item); }
		inline void remove(Item& item) { remove(&item); }

		inline bool isEmpty() const { return head == nullptr; }

		inline Iterator begin() { return Iterator(head); }
		inline Iterator end() { return Iterator(); }
	};
}

#undef XLib_hookPrev
#undef XLib_hookNext