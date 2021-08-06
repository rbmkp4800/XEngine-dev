#pragma once

#include "XLib.Vectors.h"

template <typename type1, typename type2> constexpr inline vec2<type1> operator + (const vec2<type1>& a, const vec2<type2>& b) { return vec2<type1>(a.x + b.x, a.y + b.y); }
template <typename type1, typename type2> constexpr inline vec2<type1> operator - (const vec2<type1>& a, const vec2<type2>& b) { return vec2<type1>(a.x - b.x, a.y - b.y); }
template <typename type1, typename type2> constexpr inline vec2<type1> operator / (const vec2<type1>& a, const vec2<type2>& b) { return vec2<type1>(a.x / b.x, a.y / b.y); }
template <typename type1, typename type2> constexpr inline vec2<type1> operator * (const vec2<type1>& a, const vec2<type2>& b) { return vec2<type1>(a.x * b.x, a.y * b.y); }
template <typename type1, typename type2> constexpr inline vec2<type1> operator / (const vec2<type1>& a, const type2& b) { return vec2<type1>(a.x / b, a.y / b); }
template <typename type1, typename type2> constexpr inline vec2<type1> operator * (const vec2<type1>& a, const type2& b) { return vec2<type1>(a.x * b, a.y * b); }
template <typename type1, typename type2> constexpr inline vec2<type1> operator << (const vec2<type1>& a, const type2& b) { return vec2<type1>(a.x << b, a.y << b); }
template <typename type1, typename type2> constexpr inline vec2<type1> operator >> (const vec2<type1>& a, const type2& b) { return vec2<type1>(a.x >> b, a.y >> b); }
template <typename type1, typename type2> inline vec2<type1>& operator += (vec2<type1>& a, const vec2<type2>& b) { a.x += b.x; a.y += b.y; return a; }
template <typename type1, typename type2> inline vec2<type1>& operator -= (vec2<type1>& a, const vec2<type2>& b) { a.x -= b.x; a.y -= b.y; return a; }
template <typename type1, typename type2> inline vec2<type1>& operator /= (vec2<type1>& a, const vec2<type2>& b) { a.x /= b.x; a.y /= b.y; return a; }
template <typename type1, typename type2> inline vec2<type1>& operator *= (vec2<type1>& a, const vec2<type2>& b) { a.x *= b.x; a.y *= b.y; return a; }
template <typename type1, typename type2> inline vec2<type1>& operator /= (vec2<type1>& a, const type2& b) { a.x /= b; a.y /= b; return a; }
template <typename type1, typename type2> inline vec2<type1>& operator *= (vec2<type1>& a, const type2& b) { a.x *= b; a.y *= b; return a; }
template <typename type1, typename type2> inline vec2<type1>& operator <<= (vec2<type1>& a, const type2& b) { a.x <<= b; a.y <<= b; return a; }
template <typename type1, typename type2> inline vec2<type1>& operator >>= (vec2<type1>& a, const type2& b) { a.x >>= b; a.y >>= b; return a; }

template <typename type1, typename type2> constexpr inline vec3<type1> operator + (const vec3<type1>& a, const vec3<type2>& b) { return vec3<type1>(a.x + b.x, a.y + b.y, a.z + b.z); }
template <typename type1, typename type2> constexpr inline vec3<type1> operator - (const vec3<type1>& a, const vec3<type2>& b) { return vec3<type1>(a.x - b.x, a.y - b.y, a.z - b.z); }
template <typename type1, typename type2> constexpr inline vec3<type1> operator / (const vec3<type1>& a, const vec3<type2>& b) { return vec3<type1>(a.x / b.x, a.y / b.y, a.z / b.z); }
template <typename type1, typename type2> constexpr inline vec3<type1> operator * (const vec3<type1>& a, const vec3<type2>& b) { return vec3<type1>(a.x * b.x, a.y * b.y, a.z * b.z); }
template <typename type1, typename type2> constexpr inline vec3<type1> operator / (const vec3<type1>& a, const type2& b) { return vec3<type1>(a.x / b, a.y / b, a.z / b); }
template <typename type1, typename type2> constexpr inline vec3<type1> operator * (const vec3<type1>& a, const type2& b) { return vec3<type1>(a.x * b, a.y * b, a.z * b); }
template <typename type1, typename type2> constexpr inline vec3<type1> operator << (const vec3<type1>& a, const type2& b) { return vec3<type1>(a.x << b, a.y << b, a.z << b); }
template <typename type1, typename type2> constexpr inline vec3<type1> operator >> (const vec3<type1>& a, const type2& b) { return vec3<type1>(a.x >> b, a.y >> b, a.z >> b); }
template <typename type1, typename type2> inline vec3<type1>& operator += (vec3<type1>& a, const vec3<type2>& b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
template <typename type1, typename type2> inline vec3<type1>& operator -= (vec3<type1>& a, const vec3<type2>& b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; return a; }
template <typename type1, typename type2> inline vec3<type1>& operator /= (vec3<type1>& a, const vec3<type2>& b) { a.x /= b.x; a.y /= b.y; a.z /= b.z; return a; }
template <typename type1, typename type2> inline vec3<type1>& operator *= (vec3<type1>& a, const vec3<type2>& b) { a.x *= b.x; a.y *= b.y; a.z *= b.z; return a; }
template <typename type1, typename type2> inline vec3<type1>& operator /= (vec3<type1>& a, const type2& b) { a.x /= b; a.y /= b; a.z /= b; return a; }
template <typename type1, typename type2> inline vec3<type1>& operator *= (vec3<type1>& a, const type2& b) { a.x *= b; a.y *= b; a.z *= b; return a; }
template <typename type1, typename type2> inline vec3<type1>& operator <<= (vec3<type1>& a, const type2& b) { a.x <<= b; a.y <<= b; a.z <<= b; return a; }
template <typename type1, typename type2> inline vec3<type1>& operator >>= (vec3<type1>& a, const type2& b) { a.x >>= b; a.y >>= b; a.z >>= b; return a; }

template <typename type1, typename type2> constexpr inline vec4<type1> operator + (const vec4<type1>& a, const vec4<type2>& b) { return vec4<type1>(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
template <typename type1, typename type2> constexpr inline vec4<type1> operator - (const vec4<type1>& a, const vec4<type2>& b) { return vec4<type1>(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
template <typename type1, typename type2> constexpr inline vec4<type1> operator / (const vec4<type1>& a, const vec4<type2>& b) { return vec4<type1>(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }
template <typename type1, typename type2> constexpr inline vec4<type1> operator * (const vec4<type1>& a, const vec4<type2>& b) { return vec4<type1>(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
template <typename type1, typename type2> constexpr inline vec4<type1> operator / (const vec4<type1>& a, const type2& b) { return vec4<type1>(a.x / b, a.y / b, a.z / b, a.w / b); }
template <typename type1, typename type2> constexpr inline vec4<type1> operator * (const vec4<type1>& a, const type2& b) { return vec4<type1>(a.x * b, a.y * b, a.z * b, a.w * b); }
template <typename type1, typename type2> constexpr inline vec4<type1> operator << (const vec4<type1>& a, const type2& b) { return vec4<type1>(a.x << b, a.y << b, a.z << b, a.w << b); }
template <typename type1, typename type2> constexpr inline vec4<type1> operator >> (const vec4<type1>& a, const type2& b) { return vec4<type1>(a.x >> b, a.y >> b, a.z >> b, a.w >> b); }
template <typename type1, typename type2> inline vec4<type1>& operator += (vec4<type1>& a, const vec4<type2>& b) { a.x += b.x; a.y += b.y; a.z += b.z; a.w += b.w; return a; }
template <typename type1, typename type2> inline vec4<type1>& operator -= (vec4<type1>& a, const vec4<type2>& b) { a.x -= b.x; a.y -= b.y; a.z -= b.z; a.w -= b.w; return a; }
template <typename type1, typename type2> inline vec4<type1>& operator /= (vec4<type1>& a, const vec4<type2>& b) { a.x /= b.x; a.y /= b.y; a.z /= b.z; a.w /= b.w; return a; }
template <typename type1, typename type2> inline vec4<type1>& operator *= (vec4<type1>& a, const vec4<type2>& b) { a.x *= b.x; a.y *= b.y; a.z *= b.z; a.w *= b.w; return a; }
template <typename type1, typename type2> inline vec4<type1>& operator /= (vec4<type1>& a, const type2& b) { a.x /= b; a.y /= b; a.z /= b; a.w /= b; return a; }
template <typename type1, typename type2> inline vec4<type1>& operator *= (vec4<type1>& a, const type2& b) { a.x *= b; a.y *= b; a.z *= b; a.w *= b; return a; }
template <typename type1, typename type2> inline vec4<type1>& operator <<= (vec4<type1>& a, const type2& b) { a.x <<= b; a.y <<= b; a.z <<= b; a.w <<= b; return a; }
template <typename type1, typename type2> inline vec4<type1>& operator >>= (vec4<type1>& a, const type2& b) { a.x >>= b; a.y >>= b; a.z >>= b; a.w >>= b; return a; }

template <typename type> constexpr inline vec2<type> operator - (const vec2<type>& val) { return vec2<type>(-val.x, -val.y); }
template <typename type> constexpr inline vec3<type> operator - (const vec3<type>& val) { return vec3<type>(-val.x, -val.y, -val.z); }
template <typename type> constexpr inline vec4<type> operator - (const vec4<type>& val) { return vec4<type>(-val.x, -val.y, -val.z, -val.w); }