#pragma once

using uint8 = unsigned char;
using uint16 = unsigned short int;
using uint32 = unsigned long int;
using uint64 = unsigned long long int;

using sint8 = signed char;
using sint16 = signed short int;
using sint32 = signed long int;
using sint64 = signed long long int;

using float32 = float;
using float64 = double;

using byte = uint8;

using charASCII = char;
//using charUTF8 = char8_t;
//using charUTF16 = char16_t;
//using charUTF32 = char32_t;
using wchar = wchar_t; // TODO: Remove.

#ifdef _WIN64
using uintptr = uint64;
using sintptr = sint64;
#else
using uintptr = __w64 uint32;
using sintptr = __w64 sint32;
#endif

static_assert(sizeof(uintptr) == sizeof(void*) && sizeof(sintptr) == sizeof(void*));


// Ordering ////////////////////////////////////////////////////////////////////////////////////

enum class ordering : sint8
{
	less = -1,
	equivalent = 0,
	greater = 1,
};

template <typename LeftT, typename RightT>
inline constexpr ordering compare(const LeftT& left, const RightT& right)
{
	if (left < right) return ordering::less;
	if (left > right) return ordering::greater;
	return ordering::equivalent;
}


// Logical utils ///////////////////////////////////////////////////////////////////////////////

#define imply(a, b) (!(a) || (b)) // a -> b


// Type utils //////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	namespace Internal
	{
		template <typename T> struct RemoveReferenceHelper abstract final { using ResultType = T; };
		template <typename T> struct RemoveReferenceHelper<T&> abstract final { using ResultType = T; };
		template <typename T> struct RemoveReferenceHelper<T&&> abstract final { using ResultType = T; };

		template <typename T1, typename T2> struct IsTypeEqualHelper { static constexpr bool Value = false; };
		template <typename T> struct IsTypeEqualHelper<T, T> { static constexpr bool Value = true; };
	}
	template <typename T> using RemoveReference = typename Internal::RemoveReferenceHelper<T>::ResultType;

	template <typename T> inline RemoveReference<T>&& AsRValue(T&& object) { return (RemoveReference<T>&&)object; }

	template <typename T> inline T&& ForwardRValue(RemoveReference<T>& value) { return (T&&)value; }
	template <typename T> inline T&& ForwardRValue(RemoveReference<T>&& value) { return (T&&)value; }

	template <typename T1, typename T2>
	concept IsTypeEqual = Internal::IsTypeEqualHelper<T1, T2>::Value;
}


// Construction utils //////////////////////////////////////////////////////////////////////////

namespace XLib::Internal
{
	struct PlacementNewToken {};
}

inline void* operator new (size_t, XLib::Internal::PlacementNewToken, void* block) { return block; }
inline void operator delete (void* block, XLib::Internal::PlacementNewToken, void*) {}

// TODO: Remove this.
#define XConstruct(value, ...) (new (::XLib::Internal::PlacementNewToken(), &(value)) XLib::RemoveReference<decltype(value)>(__VA_ARGS__))
#define XDestruct(value) { using XLibDestructHelperType = XLib::RemoveReference<decltype(value)>; (value).~XLibDestructHelperType(); }


// Data utils //////////////////////////////////////////////////////////////////////////////////

template <typename T, uintptr size>
constexpr auto countOf(T(&)[size])
{
	if constexpr (size <= uint8(-1))
		return uint8(size);
	else if constexpr (size <= uint16(-1))
		return uint16(size);
	else if constexpr (size <= uint32(-1))
		return uint32(size);
	else
		return size;
}

#define offsetOf(type, field) uintptr(&((type*)nullptr)->field)

template <typename T>
inline void swap(T& a, T& b)
{
	T tmp(XLib::AsRValue(a));
	a = XLib::AsRValue(b);
	b = XLib::AsRValue(tmp);
}

template <typename TargetType, typename ArgType>
inline XLib::RemoveReference<TargetType> as(ArgType&& value)
{
	static_assert(sizeof(TargetType) == sizeof(ArgType));
	return *((TargetType*)&value);
}

template <typename TargetType, typename ArgType>
inline XLib::RemoveReference<TargetType>& as(ArgType& value)
{
	static_assert(sizeof(TargetType) == sizeof(ArgType));
	return *((TargetType*)&value);
}


// Math utils //////////////////////////////////////////////////////////////////////////////////

#undef min
#undef max
template <typename T> constexpr inline T min(const T& a, const T& b) { return a < b ? a : b; }
template <typename T> constexpr inline T max(const T& a, const T& b) { return a > b ? a : b; }
template <typename T> constexpr inline T min(const T& a, const T& b, const T& c) { return min(min(a, b), c); }
template <typename T> constexpr inline T max(const T& a, const T& b, const T& c) { return max(max(a, b), c); }

template <typename T> constexpr inline T divRoundUp(const T& num, const T& denom) { return (num + denom - 1) / denom; }

template <typename T> constexpr inline T alignUp(const T& value, const T& alignment) { return T(value + alignment - 1) / alignment * alignment; }
template <typename T> constexpr inline T alignDown(const T& value, const T& alignment) { return value - (value % alignment); }

template <typename T> constexpr inline T sqr(const T& a) { return a * a; }
template <typename T> constexpr inline T abs(const T& a) { return a >= T(0) ? a : -a; }

template <typename T> constexpr inline T clamp(const T& x, const T& left, const T& right) { return max(min(x, right), left); }
template <typename T> constexpr inline T saturate(const T& x) { return clamp(x, T(0), T(1)); }

template <typename VT, typename XT>
constexpr inline VT lerp(const VT& left, const VT& right, const XT& x) { return left + (right - left) * x; }

template <typename T> constexpr inline T unlerp(const T& left, const T& right, const T& x) { return (left - x) / (left - right); }

template <typename VT, typename XT>
constexpr inline VT remap(const XT& x, const XT& fromLeft, const XT& fromRight, const VT& toLeft, const VT& toRight) { return lerp(toLeft, toRight, unlerp(fromLeft, fromRight, x)); }

// TODO:

#if 0

template<typename R = void, typename T1, typename T2>
auto min_(T1 v1, T2 v2)
{
	if constexpr (std::is_same_v<R, void>)
		return v1 < v2 ? v1 : v2;
	else
		return R(v1 < v2 ? v1 : v2);
}

template<typename R = void, typename T1, typename T2>
auto min(const T1& a, const T2 b)
{
	return XLIB_OPTIONALLY_CAST_EXPR(R, a < b ? a : b);
}

#endif


// Char utils //////////////////////////////////////////////////////////////////////////////////

namespace XLib
{
	// TODO: Should it really be here?
	class Char abstract final
	{
	public:
		static inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }
		static inline bool IsLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
		static inline bool IsLetterOrDigit(char c) { return IsLetter(c) || IsDigit(c); }
		static inline bool IsLower(char c) { return c >= 'a' && c <= 'z'; }
		static inline bool IsUpper(char c) { return c >= 'A' && c <= 'Z'; }
		static inline bool IsWhitespace(char c) { return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v'; }

		static inline char ToUpper(char c) { return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c; }
		static inline char ToLower(char c) { return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }
	};
}


// Bitwise utils ///////////////////////////////////////////////////////////////////////////////

uint8 countLeadingZeros32(uint32 value);
uint8 countLeadingZeros64(uint64 value);
uint8 countTrailingZeros32(uint32 value);
uint8 countTrailingZeros64(uint64 value);


// Memory utils ////////////////////////////////////////////////////////////////////////////////

void memorySet(void* memory, byte value, uintptr size);
void memoryCopy(void* destination, const void* source, uintptr size);
void memoryMove(void* destination, const void* source, uintptr size);


// Debug ///////////////////////////////////////////////////////////////////////////////////////

// TODO: Dev/Master variations.

namespace XLib
{
	class Debug abstract final
	{
	public:
		using FailureHandler = void(*)(const char* message);

	private:
		static FailureHandler failureHandler;

	public:
		static void Output(const char* messageCStr);

		static void DefaultFailureHandler(const char* messageCStr);

		static inline void OverrideFailureHandler(FailureHandler handler) { failureHandler = handler ? handler : DefaultFailureHandler; }
		static inline void Fail(const char* messageCStr = nullptr) { failureHandler(messageCStr); }
	};
}

#define XMacroToStrHelper(x) #x
#define XMacroToStr(x) XMacroToStrHelper(x)

#define XFailureMessagePrefix __FILE__ "(" XMacroToStr(__LINE__) "): "

#define XAssert(expression) do { if (!(expression)) { XLib::Debug::Fail(XFailureMessagePrefix "Assertion failed: `" #expression "`\n"); } } while (false)
#define XAssertUnreachableCode() { XLib::Debug::Fail(XFailureMessagePrefix "Assertion failed: Unreachable code reached\n"); }
#define XAssertNotImplemented() { XLib::Debug::Fail(XFailureMessagePrefix "Assertion failed: Not implemented\n"); }


inline uint8	XCheckedCastU8	(uint64 a) { XAssert(a <= uint8(-1)); return uint8(a); }
inline uint16	XCheckedCastU16	(uint64 a) { XAssert(a <= uint16(-1)); return uint16(a); }
inline uint32	XCheckedCastU32	(uint64 a) { XAssert(a <= uint32(-1)); return uint32(a); }


////////////////////////////////////////////////////////////////////////////////////////////////

#define XDefineEnumFlagOperators(T, IntT) \
	inline constexpr T operator & (T a, T b) { return T(IntT(a) & IntT(b)); }	\
	inline constexpr T operator | (T a, T b) { return T(IntT(a) | IntT(b)); }	\
	inline constexpr T operator ^ (T a, T b) { return T(IntT(a) ^ IntT(b)); }	\
	inline constexpr T& operator &= (T &a, T b) { ((IntT&)a) &= IntT(b); return a; }	\
	inline constexpr T& operator |= (T &a, T b) { ((IntT&)a) |= IntT(b); return a; }	\
	inline constexpr T& operator ^= (T &a, T b) { ((IntT&)a) ^= IntT(b); return a; }	\
	inline constexpr T operator ~ (T a) { return T(~IntT(a)); }

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define XTODO(todo_message) __pragma(message( __FILE__ "(" STRINGIZE(__LINE__) "): @TODO: " todo_message))




////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
namespace XLib::Internal
{
	template <typename T> struct IsIntHelper { static constexpr bool Result = false; };

	template <> struct IsIntHelper<unsigned char> { static constexpr bool Result = true; };
	template <> struct IsIntHelper<signed char> { static constexpr bool Result = true; };
	template <> struct IsIntHelper<unsigned short int> { static constexpr bool Result = true; };
	template <> struct IsIntHelper<signed short int> { static constexpr bool Result = true; };
	template <> struct IsIntHelper<unsigned int> { static constexpr bool Result = true; };
	template <> struct IsIntHelper<signed int> { static constexpr bool Result = true; };
	template <> struct IsIntHelper<unsigned long int> { static constexpr bool Result = true; };
	template <> struct IsIntHelper<signed long int> { static constexpr bool Result = true; };
	template <> struct IsIntHelper<unsigned long long int> { static constexpr bool Result = true; };
	template <> struct IsIntHelper<signed long long int> { static constexpr bool Result = true; };
}

template <typename T>
static constexpr bool isInt = XLib::Internal::IsIntHelper<T>::Result;
#endif
