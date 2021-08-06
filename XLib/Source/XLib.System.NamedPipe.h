#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.Delegate.h"
#include "XLib.System.AsyncIO.h"

namespace XLib
{
	using NamedPipeConnectedHandler = Delegate<void, bool, uintptr>;

	class NamedPipe : public XLib::NonCopyable
	{
	private:
		void *handle;

	public:
		inline NamedPipe() : handle(nullptr) {}
		inline ~NamedPipe() { destroy(); }

		void destroy();

		// server
		bool create(const char* name, uint32 outBufferSize = 4096, uint32 inBufferSize = 4096);
		bool connect();
		bool asyncConnect(DispatchedAsyncTask& task, NamedPipeConnectedHandler handler, uintptr key = 0);

		// client
		bool open(const char* name);

		// global
		void cancelIO();
		bool read(void* buffer, uint32 size);
		bool write(const void* buffer, uint32 size);
		bool asyncRead(void* buffer, uint32 size, DispatchedAsyncTask& task,
			TransferCompletedHandler handler, uintptr key = 0);
		bool asyncWrite(const void* buffer, uint32 size, DispatchedAsyncTask& task,
			TransferCompletedHandler handler, uintptr key = 0);

		template <typename Type> inline bool read(Type& value) { return read(&value, sizeof(value)); }
		template <typename Type> inline bool write(const Type& value) { return write(&value, sizeof(value)); }

		template <typename Type>
		inline bool asyncRead(Type& value, DispatchedAsyncTask& task,
			TransferCompletedHandler handler, uintptr key = 0)
			{ return asyncRead(&value, sizeof(value), task, handler, key); }

		template <typename Type>
		inline bool asyncWrite(const Type& value, DispatchedAsyncTask& task,
			TransferCompletedHandler handler, uintptr key = 0)
			{ return asyncWrite(&value, sizeof(value), task, handler, key); }

		inline bool isInitialized() { return handle != nullptr; }
		inline void* getHandle() { return handle; }
	};
}