#include <WinSock2.h>
#include <Mswsock.h>

#include "XLib.System.Network.Socket.h"

#include "XLib.Debug.h"

using namespace XLib;

static_assert(uint32(ProtocolType::TCP) == SOCK_STREAM, "XLib.System.Network.Socket.ProtocolType::TCP != SOCK_STREAM");
static_assert(uint32(ProtocolType::UDP) == SOCK_DGRAM, "XLib.System.Network.Socket.ProtocolType::UDP != SOCK_DGRAM");
static_assert(sizeof(TransferBufferSegment) == sizeof(WSABUF), "TransferBufferSegment size must be equal to WSABUF");

inline sockaddr_in IPAddressToSockaddrIn(IPAddress address, uint16 port)
{
	sockaddr_in addr = {};
	addr.sin_family = ADDRESS_FAMILY(AddressFamily::IPv4);
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = address.address.ipv4.address;
	return addr;
}
inline IPAddress SockaddrInToIPAddress(sockaddr_in addr)
{
	IPAddress address;
	address.family = AddressFamily::IPv4;
	address.address.ipv4.address = addr.sin_addr.s_addr;
	return address;
}

inline WSABUF WSABuf(void* buffer, uint32 size)
{
	WSABUF buf;
	buf.buf = (CHAR*)buffer;
	buf.len = size;
	return buf;
}

//------------------------------------------------------------------------//
//-------------------------------- Socket --------------------------------//

bool Sockets::Startup()
{
	WSAData wsaData = {};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	return true;
}

Socket::~Socket()
{
	if (handle)
	{
		closesocket(SOCKET(handle));
		handle = nullptr;
	}
}

bool Socket::initialize(AddressFamily family, ProtocolType type)
{
	static_assert(sizeof(handle) == sizeof(SOCKET),
		"XLib.System.Network.Socket.handle size != SOCKET size");

	destroy();

	void *newHandle = (void*)WSASocket(int(family), int(type),
		IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (SOCKET(newHandle) == INVALID_SOCKET)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}

	handle = newHandle;
	return true;
}

bool Socket::bind(IPAddress address, uint16 port)
{
	sockaddr_in addr = IPAddressToSockaddrIn(address, port);
	int result = ::bind(SOCKET(handle), (sockaddr*)&addr, sizeof(addr));
	if (result == SOCKET_ERROR)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	return true;
}

//------------------------------------------------------------------------//
//------------------------------- TCPSocket ------------------------------//

bool TCPSocket::connect(IPAddress address, uint16 port)
{
	sockaddr_in destAddr = IPAddressToSockaddrIn(address, port);
	for (;;)
	{
		if (!::connect(SOCKET(handle), (sockaddr*)&destAddr, sizeof(destAddr)))
			break;

		DWORD error = GetLastError();
		if (error != WSAENETUNREACH && error != WSAEHOSTUNREACH &&
			error != WSAETIMEDOUT && error != WSAECONNREFUSED)
		{
			//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
			return false;
		}
	}
	return true;
}

bool TCPSocket::send(void* buffer, uint32 size)
{
	WSABUF wsaBuf;
	wsaBuf.len = size;
	wsaBuf.buf = (char*)buffer;

	DWORD transferredSize = 0;
	int result = WSASend(SOCKET(handle), &wsaBuf, 1, &transferredSize, 0, nullptr, nullptr);

	if (result)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	if (transferredSize != size)
		return false;
	return true;
}

bool TCPSocket::sendSegments(TransferBufferSegment* segments, uint32 segmentCount)
{
	DWORD transferredSize = 0;
	DWORD flags = 0;

	int result = WSASend(SOCKET(handle), (WSABUF*)segments, segmentCount,
		&transferredSize, 0, nullptr, nullptr);

	if (result || transferredSize == 0)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	return true;
}

bool TCPSocket::receive(void* buffer, uint32 bufferSize, uint32& receivedSize)
{
	WSABUF wsaBuf;
	wsaBuf.len = bufferSize;
	wsaBuf.buf = (char*)buffer;

	DWORD transferredSize = 0;
	DWORD flags = 0;
	int result = WSARecv(SOCKET(handle), &wsaBuf, 1, &transferredSize, &flags, nullptr, nullptr);

	receivedSize = transferredSize;
	if (result || receivedSize == 0)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	return true;
}

bool TCPSocket::receiveAll(void* buffer, uint32 size)
{
	WSABUF wsaBuf;
	wsaBuf.len = size;
	wsaBuf.buf = (char*)buffer;

	DWORD transferredSize = 0;
	DWORD flags = MSG_WAITALL;
	int result = WSARecv(SOCKET(handle), &wsaBuf, 1, &transferredSize, &flags, nullptr, nullptr);

	if (result || transferredSize != size)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	return true;
}

void TCPSocket::asyncSend(void* buffer, uint32 size, DispatchedAsyncTask& task,
	TransferCompletedHandler handler, uintptr key)
{
	task.rawHandler = handler.toRaw();
	task.key = key;
	task.state = DispatchedAsyncTask::State::Transfer;

	WSABUF wsaBuf;
	wsaBuf.len = size;
	wsaBuf.buf = (char*)buffer;

	int result = WSASend(SOCKET(handle), &wsaBuf, 1, nullptr, 0, (OVERLAPPED*)&task.overlapped, nullptr);

	if (result && WSAGetLastError() != WSA_IO_PENDING)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		task.clear();
		handler.call(false, 0, key);
	}
}

void TCPSocket::asyncSendSegments(TransferBufferSegment* segments, uint32 segmentCount,
	DispatchedAsyncTask& task, TransferCompletedHandler handler, uintptr key)
{
	task.rawHandler = handler.toRaw();
	task.key = key;
	task.state = DispatchedAsyncTask::State::Transfer;

	int result = WSASend(SOCKET(handle), (WSABUF*)segments, segmentCount,
		nullptr, 0, (OVERLAPPED*)&task.overlapped, nullptr);

	if (result && WSAGetLastError() != WSA_IO_PENDING)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		task.clear();
		handler.call(false, 0, key);
	}
}

void TCPSocket::asyncReceive(void* buffer, uint32 size, DispatchedAsyncTask& task,
	TransferCompletedHandler handler, bool waitAll, uintptr key)
{
	task.rawHandler = handler.toRaw();
	task.key = key;
	task.state = DispatchedAsyncTask::State::Transfer;

	WSABUF wsaBuf;
	wsaBuf.len = size;
	wsaBuf.buf = (char*)buffer;

	DWORD flags = waitAll ? MSG_WAITALL : 0;
	int result = WSARecv(SOCKET(handle), &wsaBuf, 1, nullptr, &flags, (OVERLAPPED*)&task.overlapped, nullptr);

	if (result && WSAGetLastError() != WSA_IO_PENDING)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		task.clear();
		handler.call(false, 0, key);
	}
}

//------------------------------------------------------------------------//
//---------------------------- TCPListenSocket ---------------------------//

bool TCPListenSocket::start(uint32 backlog)
{
	int result = ::listen(SOCKET(handle), backlog);
	if (result == SOCKET_ERROR)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}
	return true;
}

bool TCPListenSocket::accept(TCPSocket& socket, IPAddress& address)
{
	sockaddr_in remoteSockaddr = {};
	int remoteSockaddrLength = sizeof(remoteSockaddr);
	SOCKET hAcceptedSocket = ::accept(SOCKET(handle),
		(sockaddr*)&remoteSockaddr, &remoteSockaddrLength);
	if (hAcceptedSocket == INVALID_SOCKET)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		return false;
	}

	socket.fromHandle((void*)hAcceptedSocket);
	address = SockaddrInToIPAddress(remoteSockaddr);

	return true;
}

void TCPListenSocket::asyncAccept(TCPListenSocket::DispatchedAsyncTask& task,
	SocketAcceptedHandler handler, uintptr key)
{
	static_assert(sizeof(task.acceptData) == sizeof(sockaddr_in) * 2 + 32,
		"XLib.System.Network.TCPListenSocket.HostedAsyncData.acceptData size != sizeof(sockaddr_in) * 2 + 32");

	task.rawHandler = handler.toRaw();
	task.key = key;
	task.state = DispatchedAsyncTask::State::SocketAccept;
	task.acceptedSocket.initialize(AddressFamily::IPv4);

	BOOL result = AcceptEx(SOCKET(handle), SOCKET(task.acceptedSocket.getHandle()), task.acceptData,
		0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, nullptr, (OVERLAPPED*)&task.overlapped);

	if (!result && WSAGetLastError() != WSA_IO_PENDING)
	{
		//Debug::LogLastSystemError(SysErrorDbgMsgFmt);
		TCPSocket dummySocket;
		handler.call(false, dummySocket, IPv4AddressAny, key);
	}
}

IPAddress TCPListenSocket::DispatchedAsyncTask::extractIPAddress()
{
	sockaddr_in *localSockaddr = nullptr, *remoteSockaddr = nullptr;
	int localSockaddrLength = 0, remoteSockaddrLength = 0;
	GetAcceptExSockaddrs(acceptData, 0,
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
		(sockaddr**)&localSockaddr, &localSockaddrLength,
		(sockaddr**)&remoteSockaddr, &remoteSockaddrLength);
	return SockaddrInToIPAddress(*remoteSockaddr);
}

//------------------------------------------------------------------------//
//------------------------------- UDPSocket ------------------------------//
