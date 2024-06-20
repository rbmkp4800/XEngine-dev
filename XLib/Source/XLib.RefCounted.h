#pragma once

#include "XLib.h"
#include "XLib.Allocation.h"
#include "XLib.NonCopyable.h"

// NOTE: For now object should be allocated via SystemHeapAllocator.

// TODO: Thread-safety.
// TODO: Handle allocators properly.

namespace XLib
{
	template <typename T>
	class RefCountedPtr
	{
	private:
		T* ptr = nullptr;

	public:
		RefCountedPtr() = default;
		inline RefCountedPtr(T* existingPtr);
		inline RefCountedPtr(const RefCountedPtr<T>& that);
		inline RefCountedPtr(RefCountedPtr<T>&& that);
		inline ~RefCountedPtr();

		RefCountedPtr& operator = (T* newPtr);
		RefCountedPtr& operator = (const RefCountedPtr<T>& that);
		RefCountedPtr& operator = (RefCountedPtr<T>&& that);

		inline T& operator * () const { return *ptr; }
		inline T* operator -> () const { return ptr; }

		inline operator bool() const { return ptr != nullptr; }

		inline T* get() const { return ptr; }
	};

	class RefCounted abstract : public NonCopyable
	{
		template <typename T>
		friend class RefCountedPtr;

	private:
		mutable uint32 referenceCount = 0;

	private:
		inline void addReference() const;
		inline void releaseReference() const;

	protected:
		RefCounted() = default;
		virtual ~RefCounted() {}
	};
}


////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINITION //////////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	template <typename T>
	inline RefCountedPtr<T>::RefCountedPtr(T* existingPtr)
	{
		ptr = existingPtr;
		if (ptr)
			ptr->addReference();
	}

	template <typename T>
	inline RefCountedPtr<T>::RefCountedPtr(const RefCountedPtr<T>& that)
	{
		ptr = that.ptr;
		if (ptr)
			ptr->addReference();
	}

	template <typename T>
	inline RefCountedPtr<T>::RefCountedPtr(RefCountedPtr<T>&& that)
	{
		ptr = that.ptr;
		that.ptr = nullptr;
	}

	template <typename T>
	inline RefCountedPtr<T>::~RefCountedPtr()
	{
		if (ptr)
		{
			ptr->releaseReference();
			ptr = nullptr;
		}
	}

	template <typename T>
	inline RefCountedPtr<T>& RefCountedPtr<T>::operator = (T* newPtr)
	{
		if (ptr != newPtr)
		{
			T* oldPtr = ptr;
			if (newPtr)
				newPtr->addReference();
			if (oldPtr)
				oldPtr->releaseReference();
			ptr = newPtr;
		}
		return *this;
	}

	template <typename T>
	inline RefCountedPtr<T>& RefCountedPtr<T>::operator = (const RefCountedPtr<T>& that)
	{
		return *this = that.get();
	}

	template <typename T>
	inline RefCountedPtr<T>& RefCountedPtr<T>::operator = (RefCountedPtr<T>&& that)
	{
		if (this != &that)
		{
			T* oldPtr = ptr;
			ptr = that.ptr;
			that.ptr = nullptr;
			if (oldPtr)
			{
				oldPtr->releaseReference();
			}
		}
		return *this;
	}

	inline void RefCounted::addReference() const
	{
		referenceCount++;
	}

	inline void RefCounted::releaseReference() const
	{
		referenceCount--;
		if (referenceCount == 0)
		{
			destruct(*this);
			SystemHeapAllocator::Release((void*)this);
		}
	}
}
