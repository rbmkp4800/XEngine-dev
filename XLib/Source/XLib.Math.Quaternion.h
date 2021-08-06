#pragma once

#include "XLib.h"
#include "XLib.Math.Matrix3x4.h"
#include "XLib.Math.Matrix4x4.h"

// TODO: implement constexpr, lerp, products ...

namespace XLib
{
	struct Quaternion
	{
		float32 x, y, z, w;

		Quaternion() = default;
		constexpr inline Quaternion(float32 x, float32 y, float32 z, float32 w) : x(x), y(y), z(z), w(w) {}

		inline void setIdentity()
		{
			x = 0.0f;
			y = 0.0f;
			z = 0.0f;
			w = 1.0f;
		}

		inline void normalize()
		{
			float32 c = 1.0f / Math::Sqrt(x * x + y * y + z * z + w * w);
			x *= c;
			y *= c;
			z *= c;
			w *= c;
		}

		inline Quaternion getNormalized() const
		{
			Quaternion result(*this);
			result.normalize();
			return result;
		}

		// quaternion must be normalized
		inline Matrix3x4 toMatrix3x4Fast(const float32x3& translation = { 0.0f, 0.0f, 0.0f }) const
		{
			Matrix3x4 result;

			result.data[0][0] = 1.0f - (2.0f * y * y) - (2.0f * z * z);
			result.data[0][1] = (2.0f * x * y) - (2.0f * z * w);
			result.data[0][2] = (2.0f * x * z) + (2.0f * y * w);
			result.data[0][3] = translation.x;

			result.data[1][0] = (2.0f * x * y) + (2.0f * z * w);
			result.data[1][1] = 1.0f - (2.0f * x * x) - (2.0f * z * z);
			result.data[1][2] = (2.0f * z * y) - (2.0f * x * w);
			result.data[1][3] = translation.y;

			result.data[2][0] = (2.0f * x * z) - (2.0f * y * w);
			result.data[2][1] = (2.0f * z * y) + (2.0f * x * w);
			result.data[2][2] = 1.0f - (2.0f * x * x) - (2.0f * y * y);
			result.data[2][3] = translation.z;

			return result;
		}

		// quaternion must be normalized
		inline Matrix4x4 toMatrix4x4Fast(const float32x3& translation = { 0.0f, 0.0f, 0.0f }) const
		{
			Matrix4x4 result;

			result.data[0][0] = 1.0f - (2.0f * y * y) - (2.0f * z * z);
			result.data[0][1] = (2.0f * x * y) - (2.0f * z * w);
			result.data[0][2] = (2.0f * x * z) + (2.0f * y * w);
			result.data[0][3] = translation.x;

			result.data[1][0] = (2.0f * x * y) + (2.0f * z * w);
			result.data[1][1] = 1.0f - (2.0f * x * x) - (2.0f * z * z);
			result.data[1][2] = (2.0f * z * y) - (2.0f * x * w);
			result.data[1][3] = translation.y;

			result.data[2][0] = (2.0f * x * z) - (2.0f * y * w);
			result.data[2][1] = (2.0f * z * y) + (2.0f * x * w);
			result.data[2][2] = 1.0f - (2.0f * x * x) - (2.0f * y * y);
			result.data[2][3] = translation.z;

			result.data[3][0] = 0.0f;
			result.data[3][0] = 0.0f;
			result.data[3][0] = 0.0f;
			result.data[3][1] = 1.0f;

			return result;
		}

		inline Matrix3x4 toMatrix3x4(const float32x3& translation = { 0.0f, 0.0f, 0.0f }) const { return getNormalized().toMatrix3x4Fast(); }
		inline Matrix4x4 toMatrix4x4(const float32x3& translation = { 0.0f, 0.0f, 0.0f }) const { return getNormalized().toMatrix4x4Fast(); }

		static inline Quaternion Identity()
		{
			Quaternion result;
			result.setIdentity();
			return result;
		}

		static inline Quaternion FromEuler(float32 x, float32 y, float32 z)
		{
			float32 angle  = x * 0.5f;
			float32 sr = Math::Sin(angle);
			float32 cr = Math::Cos(angle);

			angle = y * 0.5f;
			float32 sp = Math::Sin(angle);
			float32 cp = Math::Cos(angle);

			angle = z * 0.5f;
			float32 sy = Math::Sin(angle);
			float32 cy = Math::Cos(angle);

			float32 cpcy = cp * cy;
			float32 spcy = sp * cy;
			float32 cpsy = cp * sy;
			float32 spsy = sp * sy;

			return {
				sr * cpcy - cr * spsy,
				cr * spcy + sr * cpsy,
				cr * cpsy - sr * spcy,
				cr * cpcy + sr * spsy,
			};
		}
	};
}