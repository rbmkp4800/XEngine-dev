#pragma once

#include "XLib.h"
#include "XLib.Vectors.h"
#include "XLib.Math.h"
#include "XLib.Vectors.Math.h"

// TODO: implement *=

namespace XLib
{
	struct Matrix3x4
	{
		float32 data[3][4];

		inline float32* operator [] (uint32 row) { return data[row]; }
		inline const float32* operator [] (uint32 row) const { return data[row]; }

		inline void clear()
		{
			data[0][0] = 0.0f;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;
			data[0][3] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = 0.0f;
			data[1][2] = 0.0f;
			data[1][3] = 0.0f;

			data[2][0] = 0.0f;
			data[2][1] = 0.0f;
			data[2][2] = 0.0f;
			data[2][3] = 0.0f;
		}
		inline void identity()
		{
			data[0][0] = 1.0f;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;
			data[0][3] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = 1.0f;
			data[1][2] = 0.0f;
			data[1][3] = 0.0f;

			data[2][0] = 0.0f;
			data[2][1] = 0.0f;
			data[2][2] = 1.0f;
			data[2][3] = 0.0f;
		}
		inline void translation(float32 x, float32 y, float32 z)
		{
			data[0][0] = 1.0f;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;
			data[0][3] = x;

			data[1][0] = 0.0f;
			data[1][1] = 1.0f;
			data[1][2] = 0.0f;
			data[1][3] = y;

			data[2][0] = 0.0f;
			data[2][1] = 0.0f;
			data[2][2] = 1.0f;
			data[2][3] = z;
		}
		inline void scale(float32 x, float32 y, float32 z)
		{
			data[0][0] = x;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;
			data[0][3] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = y;
			data[1][2] = 0.0f;
			data[1][3] = 0.0f;

			data[2][0] = 0.0f;
			data[2][1] = 0.0f;
			data[2][2] = z;
			data[2][3] = 0.0f;
		}
		inline void rotationX(float32 angle)
		{
			float32 sin = Math::Sin(angle);
			float32 cos = Math::Cos(angle);

			data[0][0] = 1.0f;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;
			data[0][3] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = cos;
			data[1][2] = -sin;
			data[1][3] = 0.0f;

			data[2][0] = 0.0f;
			data[2][1] = sin;
			data[2][2] = cos;
			data[2][3] = 0.0f;
		}
		inline void rotationY(float32 angle)
		{
			float32 sin = Math::Sin(angle);
			float32 cos = Math::Cos(angle);

			data[0][0] = cos;
			data[0][1] = 0.0f;
			data[0][2] = sin;
			data[0][3] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = 1.0f;
			data[1][2] = 0.0f;
			data[1][3] = 0.0f;

			data[2][0] = -sin;
			data[2][1] = 0.0f;
			data[2][2] = cos;
			data[2][3] = 0.0f;
		}
		inline void rotationZ(float32 angle)
		{
			float32 sin = Math::Sin(angle);
			float32 cos = Math::Cos(angle);

			data[0][0] = cos;
			data[0][1] = -sin;
			data[0][2] = 0.0f;
			data[0][3] = 0.0f;

			data[1][0] = sin;
			data[1][1] = cos;
			data[1][2] = 0.0f;
			data[1][3] = 0.0f;

			data[2][0] = 0.0f;
			data[2][1] = 0.0f;
			data[2][2] = 1.0f;
			data[2][3] = 0.0f;
		}
		/*inline void lookAtCentered(const float32x3& eye, const float32x3& centeredAt, const float32x3& up)
		{
		float32x3 zaxis = -VectorMath::Normalize(centeredAt);
		float32x3 xaxis = VectorMath::Normalize(VectorMath::Cross(up, zaxis));
		float32x3 yaxis = VectorMath::Cross(zaxis, xaxis);

		data[0][0] = xaxis.x;
		data[0][1] = yaxis.x;
		data[0][2] = zaxis.x;
		data[0][3] = 0.0f;

		data[1][0] = xaxis.y;
		data[1][1] = yaxis.y;
		data[1][2] = zaxis.y;
		data[1][3] = 0.0f;

		data[2][0] = xaxis.z;
		data[2][1] = yaxis.z;
		data[2][2] = zaxis.z;
		data[2][3] = 0.0f;

		data[3][0] = VectorMath::Dot(xaxis, eye);
		data[3][1] = VectorMath::Dot(yaxis, eye);
		data[3][2] = VectorMath::Dot(zaxis, eye);
		data[3][3] = 1.0f;
		}
		inline void lookAt(const float32x3& eye, const float32x3& at, const float32x3& up)
		{
		lookAtCentered(eye, at - eye, up);
		}*/

		static inline Matrix3x4 Identity()
		{
			Matrix3x4 result;
			result.identity();
			return result;
		}
		static inline Matrix3x4 Translation(float32 x, float32 y, float32 z)
		{
			Matrix3x4 result;
			result.translation(x, y, z);
			return result;
		}
		static inline Matrix3x4 Translation(float32x3 vector)
		{
			Matrix3x4 result;
			result.translation(vector.x, vector.y, vector.z);
			return result;
		}
		static inline Matrix3x4 Scale(float32 x, float32 y, float32 z)
		{
			Matrix3x4 result;
			result.scale(x, y, z);
			return result;
		}
		static inline Matrix3x4 Scale(float32x3 vector)
		{
			Matrix3x4 result;
			result.scale(vector.x, vector.y, vector.z);
			return result;
		}
		static inline Matrix3x4 RotationX(float32 angle)
		{
			Matrix3x4 result;
			result.rotationX(angle);
			return result;
		}
		static inline Matrix3x4 RotationY(float32 angle)
		{
			Matrix3x4 result;
			result.rotationY(angle);
			return result;
		}
		static inline Matrix3x4 RotationZ(float32 angle)
		{
			Matrix3x4 result;
			result.rotationZ(angle);
			return result;
		}
		/*static inline Matrix3x4 LookAt(const float32x3& eye, const float32x3& at, const float32x3& up)
		{
		Matrix3x4 result;
		result.lookAt(eye, at, up);
		return result;
		}
		static inline Matrix3x4 LookAtCentered(const float32x3& eye, const float32x3& centeredAt, const float32x3& up)
		{
		Matrix3x4 result;
		result.lookAtCentered(eye, centeredAt, up);
		return result;
		}*/
	};

	inline Matrix3x4 operator * (const Matrix3x4& a, const Matrix3x4& b)
	{
		Matrix3x4 result;

		result[0][0] = a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0];
		result[0][1] = a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1];
		result[0][2] = a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2];
		result[0][3] = a[0][0] * b[0][3] + a[0][1] * b[1][3] + a[0][2] * b[2][3] + a[0][3];

		result[1][0] = a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0];
		result[1][1] = a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1];
		result[1][2] = a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2];
		result[1][3] = a[1][0] * b[0][3] + a[1][1] * b[1][3] + a[1][2] * b[2][3] + a[1][3];

		result[2][0] = a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0];
		result[2][1] = a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1];
		result[2][2] = a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2];
		result[2][3] = a[2][0] * b[0][3] + a[2][1] * b[1][3] + a[2][2] * b[2][3] + a[2][3];

		return result;
	}

	// TODO: refactor order
	inline float32x3 operator * (const float32x3& vector, const Matrix3x4& matrix)
	{
		return
		{
			vector.x * matrix[0][0] + vector.y * matrix[0][1] + vector.z * matrix[0][2] + matrix[0][3],
			vector.x * matrix[1][0] + vector.y * matrix[1][1] + vector.z * matrix[1][2] + matrix[1][3],
			vector.x * matrix[2][0] + vector.y * matrix[2][1] + vector.z * matrix[2][2] + matrix[2][3]
		};
	}
	inline float32x4 operator * (const float32x4& vector, const Matrix3x4& matrix)
	{
		return
		{
			vector.x * matrix[0][0] + vector.y * matrix[0][1] + vector.z * matrix[0][2] + vector.w * matrix[0][3],
			vector.x * matrix[1][0] + vector.y * matrix[1][1] + vector.z * matrix[1][2] + vector.w * matrix[1][3],
			vector.x * matrix[2][0] + vector.y * matrix[2][1] + vector.z * matrix[2][2] + vector.w * matrix[2][3],
			vector.w
		};
	}
}