#pragma once

#include "XLib.h"
#include "XLib.Vectors.h"
#include "XLib.Vectors.Arithmetics.h"
#include "XLib.Math.h"

namespace XLib
{
	struct VectorMath abstract final
	{
		static inline float32 Length(const float32x2& v) { return Math::Sqrt(v.x * v.x + v.y * v.y); }
		static inline float32 Length(const float32x3& v) { return Math::Sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
		static inline float32 Length(const float32x4& v) { return Math::Sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w); }
		static inline float32x2 Normalize(const float32x2& v) { return v / VectorMath::Length(v); }
		static inline float32x3 Normalize(const float32x3& v) { return v / VectorMath::Length(v); }
		static inline float32x4 Normalize(const float32x4& v) { return v / VectorMath::Length(v); }

		template <typename type> static constexpr inline type Projection(const type& a, const type& b) { return b * (VectorMath::Dot(a, b) / VectorMath::Dot(b, b)); }
		template <typename type> static constexpr inline vec2<type> NormalRight(const vec2<type>& v) { return vec2<type>(v.y, -v.x); }
		template <typename type> static constexpr inline vec2<type> NormalLeft(const vec2<type>& v) { return vec2<type>(-v.y, v.x); }
		template <typename type> static constexpr inline type Dot(const vec2<type>& a, const vec2<type>& b) { return a.x * b.x + a.y * b.y; }
		template <typename type> static constexpr inline type Dot(const vec3<type>& a, const vec3<type>& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
		template <typename type> static constexpr inline type Dot(const vec4<type>& a, const vec4<type>& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

		template <typename type> static constexpr inline type Cross(const vec2<type>& a, const vec2<type>& b) { return a.x * b.y - a.y * b.x; }
		template <typename type> static constexpr inline vec3<type> Cross(const vec3<type>& a, const vec3<type>& b)
		{
			return vec3<type>(
				a.y * b.z - a.z * b.y,
				a.z * b.x - a.x * b.z,
				a.x * b.y - a.y * b.x);
		}

		static inline float32x2 PolarCoords_xReference(float32 angle) { return float32x2(Math::Cos(angle), Math::Sin(angle)); }
		static inline float32x2 PolarCoords_yReference(float32 angle) { return float32x2(Math::Sin(angle), Math::Cos(angle)); }

		static inline float32x3 SphericalCoords_zZenith_xReference(float32x2 angles)
		{
			return float32x3(
				Math::Cos(angles.x) * Math::Cos(angles.y),
				Math::Sin(angles.x) * Math::Cos(angles.y),
				Math::Sin(angles.y));
		}

		static inline float32x3 SphericalCoords_zZenith_xReference(float32x2 angles, float32x2& xyNormalized)
		{
			xyNormalized = { Math::Cos(angles.x), Math::Sin(angles.x) };

			return float32x3(
				Math::Cos(angles.y) * xyNormalized.x,
				Math::Cos(angles.y) * xyNormalized.y,
				Math::Sin(angles.y));
		}
	};
}