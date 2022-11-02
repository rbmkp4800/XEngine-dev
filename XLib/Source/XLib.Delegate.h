#pragma once

// works only with MSVC++
// this code will crash on GCC due to method pointer structure differences
// https://www.codeproject.com/Articles/7150/Member-Function-Pointers-and-the-Fastest-Possible
// might be usefull for GCC version: https://gcc.gnu.org/onlinedocs/gcc/Bound-member-functions.html

#include "XLib.h"

#ifndef _MSC_VER
static_assert(false, "only MSVC++ supported");
#endif 

namespace XLib
{
	class RawDelegate
	{
		template <typename ReturnType, typename ... ArgsTypes>
		friend class Delegate;

	private:
		void *object, *method;

	public:
		inline RawDelegate() : object(nullptr), method(nullptr) {}
		inline RawDelegate(void* _object, void* _method) : object(_object), method(_method) {}

		inline void* getObject() { return object; }
		inline void* getMethod() { return method; }
	};

	template <typename ReturnType, typename ... ArgsTypes>
	class Delegate
	{
	public:
		template <class Class>
		using MethodPointer = ReturnType(Class::*)(ArgsTypes ...);

	private:
		class DummyClass
		{
		public:
			int dummy;
			void* getThis() { return this; }
		};

		// platform specific - MSVC++ =======================================================//

		static_assert(sizeof(MethodPointer<DummyClass>) == sizeof(void*), "invalid environment");

		template <uint methodPointerSize, class Class>
		struct MethodPointerConverter abstract
		{
			// This type of method pointer not supported.
		};

		// single inheritance
		template <class Class>
		class MethodPointerConverter<sizeof(void*), Class> abstract
		{
		public:
			static inline void Convert(Class* _object, MethodPointer<Class> _method,
				DummyClass*& targetObject, MethodPointer<DummyClass>& targetMethod)
			{
				union
				{
					MethodPointer<Class> sourceClassMethodPointer;
					MethodPointer<DummyClass> dummyClassMethodPointer;
				};
				sourceClassMethodPointer = _method;
				targetMethod = dummyClassMethodPointer;
				targetObject = (DummyClass*)_object;
			}
		};

		// virtual/multiple inheritance
		// MSVC++ method pointer structure:
		//		codePointer
		//		uint32 thisAdjustmentDelta
		//		uint32 vtableIndex
		// replace code pointer with our dummy method that returns adjusted this
		template <class Class>
		class MethodPointerConverter<sizeof(void*) + sizeof(uint32) * 2, Class> abstract
		{
		public:
			static inline void Convert(Class* _object, MethodPointer<Class> _method,
				DummyClass*& targetObject, MethodPointer<DummyClass>& targetMethod)
			{
				using AdjustedThisGetter = void*(Class::*)(void);
				using AdjustedThisGetterDummyMethod = void*(DummyClass::*)(void);

				union
				{
					MethodPointer<Class> sourceClassMethodPointer;
					MethodPointer<DummyClass> dummyClassMethodPointer;

					AdjustedThisGetter adjustedThisGetter;
					AdjustedThisGetterDummyMethod adjustedThisGetterDummyMethod;
				};

				sourceClassMethodPointer = _method;
				targetMethod = dummyClassMethodPointer;

				adjustedThisGetterDummyMethod = &DummyClass::getThis;
				targetObject = (DummyClass*)(_object->*adjustedThisGetter)();
			}
		};

		// multiple inheritance
		// MSVC++ method pointer structure:
		//		codePointer
		//		uint32 thisAdjustmentDelta
		//
		// due to struct alignment on x64 multiple inheritance pointer is 16 bytes length,
		//    like virtual/multiple inheritance pointer, but it's ok for this solution.
		// just redirect to virtual/multiple inheritance
		// for some reasons single inheritance with virtual methods in derived class
		//    without __single_inheritance flag specified is treated 
		template <class Class>
		class MethodPointerConverter<sizeof(void*) + sizeof(uint32), Class> abstract
		{
		public:
			static inline void Convert(Class* _object, MethodPointer<Class> _method,
				DummyClass*& targetObject, MethodPointer<DummyClass>& targetMethod)
			{
				MethodPointerConverter<sizeof(void*) + sizeof(uint32) * 2, Class>::Convert(
					_object, _method, targetObject, targetMethod);
			}
		};

		// platform specific end ============================================================//

		template <typename Class>
		inline void initialize(Class* _object, MethodPointer<Class> _method)
		{
			MethodPointerConverter<sizeof(MethodPointer<Class>), Class>::Convert(_object, _method, object, method);
		}

		DummyClass *object;
		MethodPointer<DummyClass> method;

	public:
		inline Delegate() : object(nullptr), method(nullptr) {}
		inline Delegate(const RawDelegate& raw)
		{
			object = (DummyClass*)raw.object;
			*((void**)&method) = raw.method;
		}

		template <class Class> inline Delegate(Class& _object, MethodPointer<Class> _method) { initialize(&_object, _method); }
		template <class Class> inline Delegate(Class* _object, MethodPointer<Class> _method) { initialize(_object, _method); }

		void destroy() { object = nullptr; method = nullptr; }

		inline RawDelegate toRaw()
		{
			RawDelegate raw;
			raw.object = object;
			*((MethodPointer<DummyClass>*)&raw.method) = method;
			return raw;
		}

		inline ReturnType call(ArgsTypes ... args) { return (object->*method)(args ...); }

		inline bool isInitialized() { return method ? true : false; }
	};
}