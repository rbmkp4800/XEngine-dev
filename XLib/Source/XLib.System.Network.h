#pragma once

#include "XLib.h"

namespace XLib
{
	enum class AddressFamily : uint32
	{
		None = 0,
		IPv4 = 2,	// AF_INET
		IPv6 = 23,	// AF_INET6
	};

	struct IPAddress;

	struct IPv4Address
	{
		uint32 address;

		IPv4Address() = default;
		constexpr inline IPv4Address(uint32 _address) : address(_address) {}
		constexpr inline IPv4Address(uint8 a, uint8 b, uint8 c, uint8 d)
			: address(uint32(a) | (uint32(b) << 8) | (uint32(c) << 16) | (uint32(d) << 24)) {}

		inline operator IPAddress() const;
	};
	struct IPv6Address
	{
		inline operator IPAddress() const;
	};

	constexpr IPv4Address IPv4AddressAny = IPv4Address(0, 0, 0, 0);
	constexpr IPv4Address IPv4AddressLocalhost = IPv4Address(127, 0, 0, 1);
	//constexpr IPv6Address IPv6AddressAny = 

	struct IPAddress
	{
		union
		{
			IPv4Address ipv4;
			IPv6Address ipv6;
		} address;
		AddressFamily family;
	};

	inline IPv4Address::operator IPAddress() const
	{
		IPAddress result;
		result.family = AddressFamily::IPv4;
		result.address.ipv4 = *this;
		return result;
	}
	inline IPv6Address::operator IPAddress() const
	{
		IPAddress result;
		result.family = AddressFamily::IPv6;
		result.address.ipv6 = *this;
		return result;
	}

	inline bool operator == (const IPAddress& address1, const IPAddress& address2)	// only for IPv4
	{
		return address1.family == address2.family &&
			address1.address.ipv4.address == address2.address.ipv4.address;
	}

	inline bool operator != (const IPAddress& address1, const IPAddress& address2) { return !(address1 == address2); }

	class DNS abstract final
	{

	};
}