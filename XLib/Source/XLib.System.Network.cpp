#include <WinSock2.h>

#include "XLib.System.Network.h"

using namespace XLib;

static_assert(uint32(AddressFamily::IPv4) == AF_INET, "XLib.System.Networking.Socket: AddressFamily::IPv4 must be equal to AF_INET");
static_assert(uint32(AddressFamily::IPv6) == AF_INET6, "XLib.System.Networking.Socket: AddressFamily::IPv6 must be equal to AF_INET6");