#pragma once

// `ArrayList` - single dynamic storage chunk.
// `InplaceArrayList` - inplace static storage.
// `ExpandableInplaceArrayList` - acts like `InplaceArrayList` up to its capacity, then switches to dynamic mode like `ArrayList`.
// `FixedSegmentedArrayList` - array list stored in multiple fixed segments of same size. Supports push/pop to/from front.
// `FixedLogSegmentedArrayList` - array list stored in multiple fixed segments. Segment count scales as log2 of capacity.

// TODO: Introduce overflow checks for `FixedLogSegmentedArrayList`.
// TODO: Introduce `CounterType` overflows checks.
// TODO: Maybe rename `ArrayList` to `DynamicArrayList`?
// TODO: Asserts everywhere.
// TODO: Remove CounterType from DynamicArrayList and just use uint32.
// TODO: Do for `DynamicArrayList` same thing I did for `DynamicString`: `growBuffer`, `growBufferExponentially` etc.
// TODO: `InplaceArrayList` destructor currently automatically calls element desctructors for entire buffer in addition to existing manual destruction.
// TODO: Remove `IsSafe` bullshit and do manual checks if object is trivially movable/destructible.

#include "XLib.h"
#include "XLib.Allocation.h"
#include "XLib.NonCopyable.h"

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
		static constexpr CounterType IntialBufferCapacity = divRoundUp<CounterType>(IntialBufferByteSize, sizeof(Type));

	private:
		Type* buffer = nullptr;
		CounterType capacity = 0;
		CounterType size = 0;

	private:
		inline void ensureCapacity(CounterType requiredCapacity);
		inline void reallocate(CounterType newCapacity);

	public:
		ArrayList() = default;
		inline ~ArrayList() { destroy(); }

		inline ArrayList(ArrayList&& that);
		inline void operator = (ArrayList&& that);

		inline void destroy();

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
		inline uintptr getByteSize() const { return uintptr(size) * sizeof(Type); }
		inline bool isEmpty() const { return size == 0; }

		inline operator Type*() { return buffer; }
		inline operator const Type*() const { return buffer; }
		inline Type* getData() { return buffer; }
		inline const Type* getData() const { return buffer; }

		inline Type* begin() { return buffer; }
		inline Type* end() { return buffer + size; }
		inline const Type* begin() const { return buffer; }
		inline const Type* end() const { return buffer + size; }
		inline Type& front() { return buffer[0]; }
		inline Type& back() { return buffer[size - 1]; }
	};


	template <typename Type, uintptr Capacity, typename CounterType = uint16, bool IsSafe = true>
	class InplaceArrayList
	{
		static_assert(Capacity > 0);
		static_assert(Capacity == uintptr(CounterType(Capacity)));

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
		inline uintptr getByteSize() const { return uintptr(size) * sizeof(Type); }
		inline bool isEmpty() const { return size == 0; }
		inline bool isFull() const { return size == Capacity; }

		inline operator Type*() { return buffer; }
		inline operator const Type* () const { return buffer; }
		inline Type* getData() { return buffer; }
		inline const Type* getData() const { return buffer; }

		inline Type* begin() { return buffer; }
		inline Type* end() { return buffer + size; }
		inline Type& front() { return buffer[0]; }
		inline Type& back() { return buffer[size - 1]; }
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
		inline uintptr getByteSize() const { return uintptr(size) * sizeof(Type); }
		inline bool isEmpty() const { return size == 0; }

		inline operator Type*() { return buffer; }
		inline operator const Type* () const { return buffer; }
		inline Type* getData() { return buffer; }
		inline const Type* getData() const { return buffer; }

		inline Type* begin();
		inline Type* end();
	};


	template <typename Type, uintptr SegmentSize, uintptr InplaceSegmentTableSize = 1, bool IsSafe = true, typename AllocatorType = SystemHeapAllocator>
	class FixedSegmentedArrayList :
		private AllocatorAdapterBase<AllocatorType>,
		public NonCopyable
	{
	private:
		Type* segments[InplaceSegmentTableSize] = {};
		uint32 size = 0;

	public:
		FixedSegmentedArrayList() = default;
		inline ~FixedSegmentedArrayList();
	};


	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe = true, typename AllocatorType = SystemHeapAllocator>
	class FixedLogSegmentedArrayList :
		private AllocatorAdapterBase<AllocatorType>,
		public NonCopyable
	{
		static_assert(MinCapacityLog2 < MaxCapacityLog2);
		static_assert(MaxCapacityLog2 <= 32);

	private:
		using AllocatorBase = AllocatorAdapterBase<AllocatorType>;

		static constexpr uint8 MaxSegmentCount = MinCapacityLog2 - MaxCapacityLog2 + 1;

		struct RealIndex;
		static inline RealIndex ConvertVirtualToRealIndex(uint32 virtualIndex);
		static inline uint32 CalculateSegmentSize(uint8 segmentIndex);

	private:
		Type* segments[MaxSegmentCount] = {};
		uint32 size = 0;
		uint8 allocatedSegmentCount = 0;

	private:
		inline void ensureCapacity(uint8 requiredSegmentCount);

	public:
		class Iterator
		{
		private:
			FixedLogSegmentedArrayList* parent = nullptr;
			Type* current = nullptr;
			Type* currentBlockBack = nullptr;
			uint8 currentBlockIndex = 0;

		public:
			Iterator() = default;
			~Iterator() = default;

			inline void operator ++();
			inline void operator ++(int) { operator++(0); }
			inline bool operator == (const Iterator& that) const;
			inline bool operator != (const Iterator& that) const { return !operator==(that); }
			inline Type& operator *() { return *current; }
		};

	public:
		FixedLogSegmentedArrayList() = default;
		inline ~FixedLogSegmentedArrayList();

		inline FixedLogSegmentedArrayList(FixedLogSegmentedArrayList&& that);
		inline void operator = (FixedLogSegmentedArrayList&& that);

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

		inline Iterator getIteratorAt(uint32 index) const;

		inline uint32 calculateIndex(const Type* ptr) const;

		inline Iterator begin();
		inline Iterator end();
	};
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// INLINE DEFINITIONS //////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	// ArrayList ///////////////////////////////////////////////////////////////////////////////////

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		ensureCapacity(const CounterType requiredCapacity) -> void
	{
		if (requiredCapacity <= capacity)
			return;

		// TODO: Maybe align up to 16 or some adaptive pow of 2.
		const CounterType newCapacity = max<CounterType>(requiredCapacity, capacity * 2, IntialBufferCapacity);
		reallocate(newCapacity);
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		reallocate(const CounterType newCapacity) -> void
	{
		XAssert(newCapacity > 0);
		capacity = newCapacity;

		if (!buffer)
		{
			buffer = (Type*)AllocatorBase::allocate(capacity * sizeof(Type));
			return;
		}

		if constexpr (!IsSafe)
		{
			buffer = (Type*)AllocatorBase::reallocate(buffer, capacity * sizeof(Type));
			return;
		}

		const bool expandedExistingBufferInplace =
			AllocatorBase::tryReallocateInplace(buffer, capacity * sizeof(Type));
		if (expandedExistingBufferInplace)
			return;

		// Generic case: allocate new buffer and safely move all elements

		Type* newBuffer = (Type*)AllocatorBase::allocate(capacity * sizeof(Type));

		for (CounterType i = 0; i < size; i++)
			XConstruct(newBuffer[i], AsRValue(buffer[i]));

		if (buffer)
			AllocatorBase::release(buffer);
		buffer = newBuffer;
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		operator = (ArrayList&& that) -> void
	{
		destroy();

		buffer = that.buffer;
		capacity = that.capacity;
		size = that.size;

		that.buffer = nullptr;
		that.capacity = 0;
		that.size = 0;
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		destroy() -> void
	{
		if (buffer)
		{
			for (CounterType i = 0; i < size; i++)
				XDestruct(buffer[i]);
			AllocatorBase::release(buffer);
		}
		buffer = nullptr;
		capacity = 0;
		size = 0;
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	template <typename ... ConstructorArgsTypes>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		emplaceBack(ConstructorArgsTypes&& ... constructorArgs) -> Type&
	{
		ensureCapacity(size + 1);

		Type& result = buffer[size];
		XConstruct(result, forwardRValue<ConstructorArgsTypes>(constructorArgs) ...);
		size++;

		return result;
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		pushBack(const Type& value) -> Type&
	{
		ensureCapacity(size + 1);

		Type& result = buffer[size];
		XConstruct(result, value);
		size++;

		return result;
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		popBack() -> Type
	{
		XAssert(!isEmpty());

		const CounterType elementIndex = size - 1;
		Type element = asRValue(buffer[elementIndex]);
		XDestruct(buffer[elementIndex]);
		size--;
		return element;
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
					XConstruct(buffer[i]);
			}
			else
			{
				for (CounterType i = newSize; i < size; i++)
					XDestruct(buffer[i]);
			}
		}

		size = newSize;
	}

	template <typename Type, typename CounterType, bool IsSafe, typename AllocatorType>
	inline auto ArrayList<Type, CounterType, IsSafe, AllocatorType>::
		compact() -> void
	{
		if (size == 0)
		{
			if (buffer)
				AllocatorBase::release(buffer);
			buffer = nullptr;
			capacity = 0;
		}
		else
		{
			reallocate(size);
		}
	}


	// InplaceArrayList ////////////////////////////////////////////////////////////////////////////

	template <typename Type, uintptr Capacity, typename CounterType, bool IsSafe>
	inline InplaceArrayList<Type, Capacity, CounterType, IsSafe>::
		~InplaceArrayList()
	{
		for (CounterType i = 0; i < size; i++)
			XDestruct(buffer[i]);
		size = 0;
	}

	template <typename Type, uintptr Capacity, typename CounterType, bool IsSafe>
	template <typename ... ConstructorArgsTypes>
	inline auto InplaceArrayList<Type, Capacity, CounterType, IsSafe>::
		emplaceBack(ConstructorArgsTypes&& ... constructorArgs) -> Type&
	{
		XAssert(!isFull());
		Type& result = buffer[size];
		XConstruct(result, forwardRValue<ConstructorArgsTypes>(constructorArgs) ...);
		size++;

		return result;
	}

	template <typename Type, uintptr Capacity, typename CounterType, bool IsSafe>
	inline auto InplaceArrayList<Type, Capacity, CounterType, IsSafe>::
		pushBack(const Type& value) -> Type&
	{
		XAssert(!isFull());
		Type& result = buffer[size];
		XConstruct(result, value);
		size++;

		return result;
	}

	template <typename Type, uintptr Capacity, typename CounterType, bool IsSafe>
	inline auto InplaceArrayList<Type, Capacity, CounterType, IsSafe>::
		pushBack(Type&& value) -> Type&
	{
		XAssert(!isFull());
		Type& result = buffer[size];
		XConstruct(result, AsRValue(value));
		size++;

		return result;
	}

	template <typename Type, uintptr Capacity, typename CounterType, bool IsSafe>
	inline auto InplaceArrayList<Type, Capacity, CounterType, IsSafe>::
		popBack() -> Type
	{
		XAssert(!isEmpty());
		size--;
		return asRValue(buffer[size]);
	}

	template <typename Type, uintptr Capacity, typename CounterType, bool IsSafe>
	inline auto InplaceArrayList<Type, Capacity, CounterType, IsSafe>::
		resize(CounterType newSize) -> void
	{
		for (CounterType i = newSize; i < size; i++)
			XDestruct(buffer[i]);
		for (CounterType i = size; i < newSize; i++)
			XConstruct(buffer[i]);
		size = newSize;
	}


	// ExpandableInplaceArrayList //////////////////////////////////////////////////////////////////


	// FixedSegmentedArrayList /////////////////////////////////////////////////////////////////////


	// FixedLogSegmentedArrayList //////////////////////////////////////////////////////////////////

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	struct FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::RealIndex
	{
		uint32 offset;
		uint8 segmentIndex;
	};

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		ConvertVirtualToRealIndex(const uint32 virtualIndex) -> RealIndex
	{
		const uint8 segmentIndex = 32 - countLeadingZeros32(virtualIndex >> MinCapacityLog2);
		const uint8 offsetMaskNumBits = max<sint8>(segmentIndex, 1) + sint8(MinCapacityLog2 - 1);
		const uint32 offset = virtualIndex & ~(~uint32(0) << offsetMaskNumBits);
		return RealIndex { offset, segmentIndex };
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		CalculateSegmentSize(const uint8 segmentIndex) -> uint32
	{
		const uint8 segmentSizeLog2 = max<sint8>(segmentIndex, 1) + sint8(MinCapacityLog2 - 1);
		return 1 << segmentSizeLog2;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		ensureCapacity(const uint8 requiredSegmentCount) -> void
	{
		XAssert(requiredSegmentCount <= MaxSegmentCount);

		if (requiredSegmentCount <= allocatedSegmentCount)
			return;

		for (uint8 i = allocatedSegmentCount; i < requiredSegmentCount; i++)
		{
			XAssert(!segments[i]);
			segments[i] = (Type*)AllocatorBase::allocate(CalculateSegmentSize(i) * sizeof(Type));
		}

		allocatedSegmentCount = requiredSegmentCount;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		~FixedLogSegmentedArrayList()
	{
		clear();
		compact();
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	template <typename ... ConstructorArgsTypes>
	inline auto FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		emplaceBack(ConstructorArgsTypes&& ... args) -> Type&
	{
		const RealIndex realIndex = ConvertVirtualToRealIndex(size);
		ensureCapacity(realIndex.segmentIndex + 1);
		
		Type& result = segments[realIndex.segmentIndex][realIndex.offset];
		XConstruct(result, forwardRValue<ConstructorArgsTypes>(args) ...);
		size++;

		return result;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		pushBack(const Type& value) -> Type&
	{
		const RealIndex realIndex = ConvertVirtualToRealIndex(size);
		ensureCapacity(realIndex.segmentIndex + 1);

		Type& result = segments[realIndex.segmentIndex][realIndex.offset];
		if constexpr (IsSafe)
			XConstruct(result, value);
		else
			result = value;
		size++;

		return result;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		popBack() -> Type
	{

	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		resize(const uint32 newSize) -> void
	{
		const RealIndex newEndRealIndex = ConvertVirtualToRealIndex(newSize);
		ensureCapacity(newEndRealIndex.segmentIndex + 1);

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
	inline auto FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		compact() -> void
	{
		uint8 requiredSegmentCount = 0;
		if (size > 0)
			requiredSegmentCount = ConvertVirtualToRealIndex(size - 1).segmentIndex + 1;

		XAssert(requiredSegmentCount <= allocatedSegmentCount);
		for (uint8 i = requiredSegmentCount; i < allocatedSegmentCount; i++)
		{
			XAssert(segments[i]);
			AllocatorBase::release(segments[i]);
			segments[i] = nullptr;
		}

		allocatedSegmentCount = requiredSegmentCount;
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		reserve(const uint32 newCapacity) -> void
	{
		if (!newCapacity)
			return;

		const uint8 requiredSegmentCount = ConvertVirtualToRealIndex(newCapacity - 1).segmentIndex + 1;
		ensureCapacity(requiredSegmentCount);
	}

	template <typename Type, uint8 MinCapacityLog2, uint8 MaxCapacityLog2, bool IsSafe, typename AllocatorType>
	inline auto FixedLogSegmentedArrayList<Type, MinCapacityLog2, MaxCapacityLog2, IsSafe, AllocatorType>::
		operator [] (const uint32 index) -> Type&
	{
		const RealIndex realIndex = ConvertVirtualToRealIndex(index);
		return segments[realIndex.segmentIndex][realIndex.offset];
	}
}
