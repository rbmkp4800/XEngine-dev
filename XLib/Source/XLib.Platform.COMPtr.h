#pragma once

#include <guiddef.h>

#include "XLib.NonCopyable.h"

namespace XLib::Platform
{
	namespace Internal
	{
		void IUnknown_AddRef(void* ptr);
		void IUnknown_Release(void* ptr);
	}

	template <typename Type>
	class COMPtr : public NonCopyable
	{
	private:
		Type *ptr = nullptr;

	public:
		COMPtr() = default;
		inline ~COMPtr() { destroy(); }

		inline COMPtr(COMPtr&& that)
		{
			destroy();
			ptr = that.ptr;
			that.ptr = nullptr;
		}
		inline void operator = (COMPtr&& that)
		{
			destroy();
			ptr = that.ptr;
			that.ptr = nullptr;
		}

		inline void destroy()
		{
			if (ptr)
			{
				Internal::IUnknown_Release(ptr);
				ptr = nullptr;
			}
		}

		inline Type* moveToPtr()
		{
			Type* result = ptr;
			ptr = nullptr;
			return result;
		}

		inline Type** initRef()
		{
			destroy();
			return &ptr;
		}
		inline void** voidInitRef() { return (void**)initRef(); }

		inline Type*const* operator & () { return &ptr; }

		inline Type* operator -> () { return ptr; }
		inline Type* operator -> () const { return ptr; }
		inline operator Type* () { return ptr; }
		inline operator Type* () const { return ptr; }
		inline void operator = (Type* value)
		{
			destroy();
			ptr = value;
			if (ptr)
				Internal::IUnknown_AddRef(ptr);
		}

		inline GUID uuid() { return __uuidof(Type); }

		inline bool isInitialized() { return ptr != nullptr; }
	};
}