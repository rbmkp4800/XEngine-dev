#pragma once

#include "XLib.h"
#include "XLib.NonCopyable.h"
#include "XLib.Delegate.h"
#include "XLib.System.Network.h"
#include "XLib.System.AsyncIO.h"

// TODO: implement proper disconnect/destroy
//		https://msdn.microsoft.com/en-us/library/windows/desktop/ms738547(v=vs.85).aspx
// TODO: implement error codes

namespace XLib
{
	class TCPSocket;
	class TCPListenSocket;
	class UDPSocket;

	enum class ProtocolType : uint32
	{
		None = 0,
		TCP = 1,	// SOCK_STREAM
		UDP = 2,	// SOCK_DGRAM
	};

	enum class SocketError : uint32
	{
		None = 0,
		SocketsSystemNotInitialized,	// WSANOTINITIALISED
		TimeOut,						// WSAETIMEDOUT
	};

	struct Sockets abstract final
	{
		static bool Startup();
	};

	using SocketAcceptedHandler = Delegate<void, bool, TCPSocket&, IPAddress, uintptr>;

	struct TransferBufferSegment // WSABUF. can't make static_assert (((
	{
		uint32 size;
		const void* buffer;

		TransferBufferSegment() = default;
		inline TransferBufferSegment(const void* _buffer, uint32 _size)
			: buffer(_buffer), size(_size) {}
	};

	class Socket abstract : public NonCopyable
	{
		friend TCPSocket;
		friend TCPListenSocket;
		friend UDPSocket;

	private:
		void *handle;

		inline void fromHandle(void* _handle)
		{
			destroy();
			handle = _handle;
		}
		bool initialize(AddressFamily family, ProtocolType type);
		bool bind(IPAddress address, uint16 port);
		void* moveToHandle()
		{
			void* result = handle;
			handle = nullptr;
			return result;
		}

	public:
		inline Socket() : handle(nullptr) {}
		~Socket();

		inline  void destroy() { this->~Socket(); }

		inline Socket(Socket&& that)
		{
			handle = that.handle;
			that.handle = nullptr;
		}
		inline void operator = (Socket&& that)
		{
			destroy();
			handle = that.handle;
			that.handle = nullptr;
		}

		inline void* getHandle() { return handle; }
		inline bool isInitialized() const { return handle != nullptr; }
	};

	class TCPSocket : public Socket
	{
	public:
		inline bool initialize(AddressFamily family) { return Socket::initialize(family, ProtocolType::TCP); }

		bool connect(IPAddress address, uint16 port);
		bool send(void* buffer, uint32 size);
		bool sendSegments(TransferBufferSegment* segments, uint32 segmentCount);
		bool receive(void* buffer, uint32 bufferSize, uint32& receivedSize);
		bool receiveAll(void* buffer, uint32 size);

		//bool asyncConnect();
		void asyncSend(void* buffer, uint32 size, DispatchedAsyncTask& task,
			TransferCompletedHandler handler, uintptr key = 0);
		void asyncSendSegments(TransferBufferSegment* segments, uint32 segmentCount,
			DispatchedAsyncTask& task, TransferCompletedHandler handler, uintptr key = 0);
		void asyncReceive(void* buffer, uint32 size, DispatchedAsyncTask& task,
			TransferCompletedHandler handler, bool waitAll = false, uintptr key = 0);

		template <uint32 segmentCount>
		inline bool sendSegments(TransferBufferSegment(&segments)[segmentCount])
			{ return sendSegments(segments, segmentCount); }
	};

	class TCPListenSocket : public Socket
	{
	public:
		class DispatchedAsyncTask : public XLib::DispatchedAsyncTask
		{
			friend AsyncIODispatcher;
			friend TCPListenSocket;

		private:
			byte acceptData[64];
			TCPSocket acceptedSocket;

			IPAddress extractIPAddress();

			inline void clear()
			{
				Memory::Set(this, 0, sizeof(*this));
			}
		};

		inline bool initialize(IPAddress address, uint16 port)
		{
			if (!Socket::initialize(address.family, ProtocolType::TCP))
				return false;
			if (!Socket::bind(address, port))
				return false;
			return true;
		}
		bool start(uint32 backlog = 5);

		bool accept(TCPSocket& socket, IPAddress& address);
		void asyncAccept(TCPListenSocket::DispatchedAsyncTask& task, SocketAcceptedHandler handler, uintptr key = 0);
	};

	class UDPSocket : public Socket
	{
	public:
		inline bool initialize(AddressFamily family) { return Socket::initialize(family, ProtocolType::UDP); }

		bool sendTo(void* data, uint32 size, IPAddress address, uint16 port);
		bool receiveFrom(void* buffer, uint32 bufferSize, uint32& datagramLength, IPAddress& address, uint16& port);
		void asyncSendTo(void* data, uint32 size, IPAddress address, uint16 port, DispatchedAsyncTask& task);
		void asyncReceiveFrom(DispatchedAsyncTask& task);
	};
}