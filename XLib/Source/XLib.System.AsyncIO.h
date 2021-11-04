#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.Delegate.h"

namespace XLib
{
	class AsyncIODispatcher;

	using TransferCompletedHandler = Delegate<void, bool, uint32, uintptr>;

	class Socket;
	class TCPSocket;
	class TCPListenSocket;
	class NamedPipe;

	class DispatchedAsyncTask : public NonCopyable
	{
		friend AsyncIODispatcher;

		friend TCPSocket;
		friend TCPListenSocket;
		friend NamedPipe;

	private:
		static constexpr uint32 overlappedSize = sizeof(void*) == 4 ? 20 : 32;

		enum class State : uint32
		{
			None = 0,
			Transfer = 0xA1B2C301,
			SocketAccept = 0xA1B2C302,
			//SocketConnect = 0xA1B2C303,
			NamedPipeConnect = 0xA1B2C304,
		};

		byte overlapped[overlappedSize] = { 0 };	// must be first
		RawDelegate rawHandler;
		uintptr key = 0;
		State state = State::None;

		inline void clear()
			{ Memory::Set(this, 0, sizeof(*this)); }

	public:
		void cancel();

		inline bool isActive() const { return state != State::None; }
	};

	class AsyncIODispatcher : public NonCopyable
	{
	private:
		void *hIOCP = nullptr;

	public:
		AsyncIODispatcher() = default;
		inline ~AsyncIODispatcher() { destroy(); }

		void initialize();
		void destroy();

		void associate(Socket& socket);
		void associate(NamedPipe& pipe);

		void dispatchAll();
		void dispatchPending();
		void cleanupQueue();

		void invokeShutdown();
		void invoke(Delegate<void> handler);

		inline bool isInitialized() { return hIOCP != nullptr; }
	};
}