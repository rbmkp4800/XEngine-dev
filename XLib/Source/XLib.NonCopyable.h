#pragma once

namespace XLib
{
	class NonCopyable abstract
	{
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator = (const NonCopyable&) = delete;

	protected:
		NonCopyable() = default;
		~NonCopyable() = default;
	};
}