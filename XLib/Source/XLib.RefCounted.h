#pragma once

#include "XLib.NonCopyable.h"

namespace XLib
{
	template <typename T>
	class RefCountedPtr
	{
	private:
		T* ptr = nullptr;

	public:
		RefCountedPtr() = default;
		inline RefCountedPtr(T* ptr);
		inline RefCountedPtr(RefCountedPtr<T>&& that);
		inline ~RefCountedPtr();

		RefCountedPtr& operator = (T* ptr);
		RefCountedPtr& operator = (const RefCountedPtr<T>& that);
		RefCountedPtr& operator = (RefCountedPtr<T>&& that);

		inline T& operator * () const;
		inline T* operator -> () const;

		inline operator bool() const { return ptr != nullptr; }

		inline T* get() const { return ptr; }
	};

	class RefCounted abstract : public NonCopyable
	{
		friend RefCountedPtr;

	private:
		mutable uint32 referenceCount = 0;

	private:
		inline uint32 addReference();
		inline uint32 releaseReference();

	protected:
		virtual ~RefCounted() = 0;
	};
}
