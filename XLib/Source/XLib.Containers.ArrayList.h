#pragma once

// `ArrayList` - single dynamic storage chunk.
// `InplaceArrayList` - inplace static storage.
// `ExpandableInplaceArrayList` - acts like `InplaceArrayList` up to its capacity, then switches to dynamic mode like `ArrayList`.
// `StaticSegmentedArrayList` - multiple static storage chunks with fast indexing.

// TODO: Introduce overflow checks for `StaticSegmentedArrayList`.
// TODO: Introduce `CounterType` overflows checks.

#include "XLib.h"
#include "XLib.AllocatorAdapterBase.h"
#include "XLib.NonCopyable.h"
#include "XLib.SystemHeapAllocator.h"

namespace XLib
{
	template <typename Type, typename CounterType = uint32, bool IsSafe = true, typename AllocatorType = SystemHeapAllocator>
	class ArrayList :
		private AllocatorAdapterBase<AllocatorType>,
		public NonCopyable
	{
	private:
		using AllocatorBase = AllocatorAdapterBase<AllocatorType>;

		static constexpr uintptr IntialBufferByteSize = 64;
		static constexpr CounterType IntialBufferCapacity = CounterType(divRoundUp(IntialBufferByteSize, sizeof(Type)));

	private:
		Type* buffer = nullptr;
		CounterType capacity = 0;
		CounterType size = 0;

	private:
		inline void ensureCapacity(CounterType requiredCapacity);

	public:
		ArrayList() = default;
		inline ~ArrayList();

		ArrayList(const ArrayList& that) = delete;
		void operator = (const ArrayList& that) = delete;

		inline ArrayList(ArrayList&& that);
		inline void operator = (ArrayList&& that);

		template <typename ... ConstructorArgsTypes>
		inline Type& emplaceBack(ConstructorArgsTypes&& ... constructorArgs);

		inline Type& pushBack(const Type& value);
		//inline Type& pushBack(Type&& value);
		inline Type popBack();

		inline void resize(CounterType newSize);
		inline void clear() { resize(0); }
		inline void compact();
		inline void reserve(CounterType newCapacity) { ensureCapacity(newCapacity); }

		inline CounterType getSize() const { return size; }
		inline bool isEmpty() const { return size == 0; }

		inline Type& front() { return buffer[0]; }
		inline Type& back() { return buffer[size - 1]; }

		inline operator Type*() { return buffer; }
		inline operator const Type*() const { return buffer; }
		inline Type* getData() { return buffer; }
		inline const Type* getData() const { return buffer; }

		inline Type* begin() { return buffer; }
		inline Type* end() { return (Type*)buffer + size; }
	};


	template <typename Type, uintptr Capacity, typename CounterType = uint16, bool IsSafe = true>
	class InplaceArrayList
	{
	private:
		Type buffer[Capacity];
		CounterType size = 0;

	public:
		InplaceArrayList() = default;
		~InplaceArrayList();

		inline InplaceArrayList(InplaceArrayList&& that);
		inline void operator = (InplaceArrayList&& that);

		template <typename ... ConstructorArgsTypes>
		inline Type& emplaceBack(ConstructorArgsTypes&& ... constructorArgs);

		inline Type& pushBack(const Type& value);
		inline Type& pushBack(Type&& value);
		inline Type popBack();

		inline void resize(CounterType newSize);
		inline void clear() { resize(0); }

		inline CounterType getSize() const { return size; }
		inline bool isEmpty() const { return size == 0; }
		inline bool isFull() const { return size == Capacity; }

		inline operator Type*() { return buffer; }
		inline operator const Type* () const { return buffer; }
		inline Type* getData() { return buffer; }
		inline const Type* getData() const { return buffer; }

		inline Type* begin();
		inline Type* end();
	};


	template <typename Type, uintptr InplaceCapacity, typename CounterType = uint32, bool IsSafe = true, typename AllocatorType = SystemHeapAllocator>
	class ExpandableInplaceArrayList :
		private AllocatorAdapterBase<AllocatorType>,
		public NonCopyable
	{
	private:
		Type buffer[InplaceCapacity];
		CounterType capacity = 0;
		CounterType size = 0;

	public:
		ExpandableInplaceArrayList() = default;
		~ExpandableInplaceArrayList();

		inline ExpandableInplaceArrayList(ExpandableInplaceArrayList&& that);
		inline void operator = (ExpandableInplaceArrayList&& that);

		template <typename ... ConstructorArgsTypes>
		inline Type& emplaceBack(ConstructorArgsTypes&& ... constructorArgs);

		inline Type& pushBack(const Type& value);
		inline Type& pushBack(Type&& value);
		inline Type popBack();

		inline void resize(CounterType newSize);
		inline void clear() { resize(0); }
		inline void compact();
		inline void reserve(CounterType newCapacity);

		inline CounterType getSize() const { return size; }
		inline bool isEmpty() const { return size == 0; }

		inline operator Type*() { return buffer; }
		inline operator const Type* () const { return buffer; }
		inline Type* getData() { return buffer; }
		inline const Type* getData() const { return buffer; }

		inline Type* begin();
		inline Type* end();
	};


	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe = true, typename AllocatorType = SystemHeapAllocator>
	class StaticSegmentedArrayList :
		private AllocatorAdapterBase<AllocatorType>,
		public NonCopyable
	{
		static_assert(MinCapacityLog2 < MaxCapacityLog2);
		static_assert(MaxCapacityLog2 <= 32);

	private:
		using AllocatorBase = AllocatorAdapterBase<AllocatorType>;

		static constexpr uint8 MaxBufferCount = MinCapacityLog2 - MaxCapacityLog2 + 1;

		struct RealIndex;
		static inline RealIndex ConvertVirtualToRealIndex(uint32 virtualIndex);
		static inline uint32 CalculateNthBufferSize(uint8 bufferIndex);

	private:
		Type* buffers[MaxBufferCount] = {};
		uint32 size;
		uint8 allocatedBufferCount = 0;

	private:
		inline void ensureCapacity(uint8 requiredBufferCount);

	public:
		class Iterator
		{
		private:
			StaticSegmentedArrayList& parent;
			Type* current;
			Type* currentBlockBack;
			uint8 currentBlockIndex;

		public:
			inline Iterator(StaticSegmentedArrayList& parent);
			~Iterator() = default;

			inline void operator ++();
			inline bool operator != (const Iterator& that) const;
			inline Type& operator *() { return *current; }
		};

	public:
		StaticSegmentedArrayList() = default;
		inline ~StaticSegmentedArrayList();

		inline StaticSegmentedArrayList(StaticSegmentedArrayList&& that);
		inline void operator = (StaticSegmentedArrayList&& that);

		template <typename ... ConstructorArgsTypes>
		inline Type& emplaceBack(ConstructorArgsTypes&& ... constructorArgs);

		inline Type& pushBack(const Type& value);
		inline Type& pushBack(Type&& value);
		inline Type popBack();

		inline void resize(uint32 newSize);
		inline void clear() { resize(0); }
		inline void compact();
		inline void reserve(uint32 newCapacity);

		inline uint32 getSize() const { return size; }
		inline bool isEmpty() const { return size == 0; }

		inline Type& operator [] (uint32 index);
		inline const Type& operator [] (uint32 index) const;

		inline uint32 calculateIndex(const Type* ptr) const;

		inline Iterator begin();
		inline Iterator end();
	};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Definition //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	// ArrayList ///////////////////////////////////////////////////////////////////////////////////

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		ensureCapacity(const CounterType requiredCapacity) -> void
	{
		if (requiredCapacity <= capacity)
			return;

		capacity = max<CounterType>(requiredCapacity, capacity * 2, IntialBufferCapacity);

		if constexpr (!IsSafe)
		{
			buffer = (Type*)AllocatorBase::reallocate(buffer, capacity * sizeof(Type));
			return;
		}

		const bool expandedExistingBufferInplace =
			AllocatorBase::reallocateInplace(buffer, capacity * sizeof(Type));
		if (expandedExistingBufferInplace)
			return;

		// Generic case: allocate new buffer and safely move all elements

		Type* newBuffer = (Type*)AllocatorBase::allocate(capacity * sizeof(Type));

		for (CounterType i = 0; i < size; i++)
			construct(newBuffer[i], asRValue(buffer[i]));

		AllocatorBase::release(buffer);
		buffer = newBuffer;
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		~ArrayList()
	{
		clear();
		compact();
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	template <typename ... ConstructorArgsTypes>
	inline Type& ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		emplaceBack(ConstructorArgsTypes&& ... args)
	{
		ensureCapacity(size + 1);

		Type& result = buffer[size];
		construct(result, forwardRValue(args) ...);
		size++;

		return result;
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		pushBack(const Type& value) -> Type&
	{
		ensureCapacity(size + 1);

		Type& result = buffer[size];
		if constexpr (IsSafe)
			construct(result, value);
		else
			result = value;
		size++;

		return result;
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		popBack() -> Type
	{
		// ???
		//...;
		//return move(buffer[--size]);
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		resize(CounterType newSize) -> void
	{
		ensureCapacity(newSize);

		if constexpr (IsSafe)
		{
			if (size < newSize)
			{
				for (CounterType i = size; i < newSize; i++)
					construct(buffer[i]);
			}
			else
			{
				for (CounterType i = newSize; i < size; i++)
					buffer[i].~Type();
			}
		}

		size = newSize;
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		compact() -> void
	{
		capacity = size;

		if (capacity == 0)
		{
			AllocatorBase::release(buffer);
			buffer = 0;

			Type* newBuffer = (Type*)AllocatorBase::reallocate(buffer, capacity * sizeof(Type));

			// TODO: Move data if buffer relocated
		}
		else
		{
			
		}
	}


	// InplaceArrayList ////////////////////////////////////////////////////////////////////////////


	// ExpandableInplaceArrayList //////////////////////////////////////////////////////////////////


	// StaticSegmentedArrayList ////////////////////////////////////////////////////////////////////

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	struct StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::RealIndex
	{
		uint32 offset;
		uint8 bufferIndex;
	};

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		ConvertVirtualToRealIndex(const uint32 virtualIndex) -> RealIndex
	{
		const uint8 bufferIndex = 32 - countLeadingZeros32(virtualIndex >> MinCapacityLog2);
		const uint8 offsetMaskNumBits = max<sint8>(bufferIndex, 1) + sint8(MinCapacityLog2 - 1);
		const uint32 offset = virtualIndex & ~(~uint32(0) << offsetMaskNumBits);
		return RealIndex { offset, bufferIndex };
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		CalculateNthBufferSize(const uint8 bufferIndex) -> uint32
	{
		const uint8 bufferSizeLog2 = max<sint8>(bufferIndex, 1) + sint8(MinCapacityLog2 - 1);
		return 1 << bufferSizeLog2;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		ensureCapacity(const uint8 requiredBufferCount) -> void
	{
		// ASSERT(requiredBufferCount <= MaxBufferCount);

		if (requiredBufferCount <= allocatedBufferCount)
			return;

		for (uint8 i = allocatedBufferCount; i < requiredBufferCount; i++)
		{
			// ASSERT(!buffers[i]);
			buffers[i] = (Type*)AllocatorBase::allocate(CalculateNthBufferSize(i));
		}

		allocatedBufferCount = requiredBufferCount;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		~StaticSegmentedArrayList()
	{
		clear();
		compact();
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	template <typename ... ConstructorArgsTypes>
	inline auto StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		emplaceBack(ConstructorArgsTypes&& ... args) -> Type&
	{
		const RealIndex realIndex = ConvertVirtualToRealIndex(size);
		ensureCapacity(realIndex.bufferIndex + 1);
		
		Type& result = buffers[realIndex.bufferIndex][realIndex.offset];
		construct(result, forwardRValue(args) ...);
		size++;

		return result;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		pushBack(const Type& value) -> Type&
	{
		const RealIndex realIndex = ConvertVirtualToRealIndex(size);
		ensureCapacity(realIndex.bufferIndex + 1);

		Type& result = buffers[realIndex.bufferIndex][realIndex.offset];
		if constexpr (IsSafe)
			construct(result, value);
		else
			result = value;
		size++;

		return result;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		popBack() -> Type
	{

	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		resize(const uint32 newSize) -> void
	{
		const RealIndex newEndRealIndex = ConvertVirtualToRealIndex(newSize);
		ensureCapacity(newEndRealIndex.bufferIndex + 1);

		if constexpr (IsSafe)
		{
			if (size < newSize)
			{
				// ...
			}
			else
			{
				// ...
			}
		}

		size = newSize;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		compact() -> void
	{
		uint8 requiredBufferCount = 0;
		if (size > 0)
			requiredBufferCount = ConvertVirtualToRealIndex(size - 1).bufferIndex + 1;

		// ASSERT(requiredBufferCount <= allocatedBufferCount);
		for (uint8 i = requiredBufferCount; i < allocatedBufferCount; i++)
		{
			// ASSERT(buffers[i]);
			AllocatorBase::release(buffers[i]);
			buffers[i] = nullptr;
		}

		allocatedBufferCount = requiredBufferCount;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		reserve(const uint32 newCapacity) -> void
	{
		if (!newCapacity)
			return;

		const uint8 requiredBufferCount = ConvertVirtualToRealIndex(newCapacity - 1).bufferIndex + 1;
		ensureCapacity(requiredBufferCount);
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto StaticSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		operator [] (const uint32 index) -> Type&
	{
		const RealIndex realIndex = ConvertVirtualToRealIndex(index);
		return buffers[realIndex.bufferIndex][realIndex.offset];
	}
}
