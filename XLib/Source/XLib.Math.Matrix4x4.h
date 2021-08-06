#pragma once

#include "XLib.h"
#include "XLib.Vectors.h"
#include "XLib.Math.h"
#include "XLib.Vectors.Math.h"

// TODO: maybe move some implementation to .cpp file
// TODO: transpose (to have same orientation as Matrix3x4) or not??
// TODO: implement *=
// TODO: refactor order

namespace XLib
{
	struct Matrix4x4
	{
		float32 data[4][4];

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

			data[3][0] = 0.0f;
			data[3][1] = 0.0f;
			data[3][2] = 0.0f;
			data[3][3] = 0.0f;
		}
		inline void transpose()
		{
			for (uint32 i = 0; i < 4; i++)
				for (uint32 j = 0; j < 4; j++)
					swap(data[i][j], data[j][i]);
		}
		inline void setIdentity()
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

			data[3][0] = 0.0f;
			data[3][1] = 0.0f;
			data[3][2] = 0.0f;
			data[3][3] = 1.0f;
		}
		inline void setTranslation(float32 x, float32 y, float32 z)
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

			data[3][0] = x;
			data[3][1] = y;
			data[3][2] = z;
			data[3][3] = 1.0f;
		}
		inline void setScale(float32 x, float32 y, float32 z)
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

			data[3][0] = 0.0f;
			data[3][1] = 0.0f;
			data[3][2] = 0.0f;
			data[3][3] = 1.0f;
		}
		inline void setRotationX(float32 angle)
		{
			float32 sin = Math::Sin(angle), cos = Math::Cos(angle);
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

			data[3][0] = 0.0f;
			data[3][1] = 0.0f;
			data[3][2] = 0.0f;
			data[3][3] = 1.0f;
		}
		inline void setRotationY(float32 angle)
		{
			float32 sin = Math::Sin(angle), cos = Math::Cos(angle);
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

			data[3][0] = 0.0f;
			data[3][1] = 0.0f;
			data[3][2] = 0.0f;
			data[3][3] = 1.0f;
		}
		inline void setRotationZ(float32 angle)
		{
			float32 sin = Math::Sin(angle), cos = Math::Cos(angle);
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

			data[3][0] = 0.0f;
			data[3][1] = 0.0f;
			data[3][2] = 0.0f;
			data[3][3] = 1.0f;
		}
		inline void setPerspective(float32 fov, float32 aspect, float32 zNear, float32 zFar)
		{
			float32 yScale = 1.0f / Math::Tan(fov / 2.0f);
			float32 xScale = yScale / aspect;

			data[0][0] = xScale;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;
			data[0][3] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = yScale;
			data[1][2] = 0.0f;
			data[1][3] = 0.0f;

			data[2][0] = 0.0f;
			data[2][1] = 0.0f;
			data[2][2] = zFar / (zFar - zNear);
			data[2][3] = 1.0f;

			data[3][0] = 0.0f;
			data[3][1] = 0.0f;
			data[3][2] = zNear * zFar / (zNear - zFar);
			data[3][3] = 0.0f;
		}
		inline void setLookAtCentered(const float32x3& position, const float32x3& centeredTarget, const float32x3& up)
		{
			float32x3 zaxis = VectorMath::Normalize(centeredTarget);
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

			data[3][0] = -VectorMath::Dot(xaxis, position);
			data[3][1] = -VectorMath::Dot(yaxis, position);
			data[3][2] = -VectorMath::Dot(zaxis, position);
			data[3][3] = 1.0f;
		}
		inline void setLookAt(const float32x3& position, const float32x3& target, const float32x3& up)
		{
			setLookAtCentered(position, target - position, up);
		}
		inline void setInverse(const Matrix4x4& a)
		{
			// https://www.geometrictools.com/Documentation/LaplaceExpansionTheorem.pdf

			float32 s0 = a[0][0] * a[1][1] - a[1][0] * a[0][1];
			float32 s1 = a[0][0] * a[1][2] - a[1][0] * a[0][2];
			float32 s2 = a[0][0] * a[1][3] - a[1][0] * a[0][3];
			float32 s3 = a[0][1] * a[1][2] - a[1][1] * a[0][2];
			float32 s4 = a[0][1] * a[1][3] - a[1][1] * a[0][3];
			float32 s5 = a[0][2] * a[1][3] - a[1][2] * a[0][3];

			float32 c5 = a[2][2] * a[3][3] - a[3][2] * a[2][3];
			float32 c4 = a[2][1] * a[3][3] - a[3][1] * a[2][3];
			float32 c3 = a[2][1] * a[3][2] - a[3][1] * a[2][2];
			float32 c2 = a[2][0] * a[3][3] - a[3][0] * a[2][3];
			float32 c1 = a[2][0] * a[3][2] - a[3][0] * a[2][2];
			float32 c0 = a[2][0] * a[3][1] - a[3][0] * a[2][1];

			float32 rdet = 1.0f / (s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0);

			data[0][0] = ( + a[1][1] * c5 - a[1][2] * c4 + a[1][3] * c3) * rdet;
			data[0][1] = ( - a[0][1] * c5 + a[0][2] * c4 - a[0][3] * c3) * rdet;
			data[0][2] = ( + a[3][1] * s5 - a[3][2] * s4 + a[3][3] * s3) * rdet;
			data[0][3] = ( - a[2][1] * s5 + a[2][2] * s4 - a[2][3] * s3) * rdet;

			data[1][0] = ( - a[1][0] * c5 + a[1][2] * c2 - a[1][3] * c1) * rdet;
			data[1][1] = ( + a[0][0] * c5 - a[0][2] * c2 + a[0][3] * c1) * rdet;
			data[1][2] = ( - a[3][0] * s5 + a[3][2] * s2 - a[3][3] * s1) * rdet;
			data[1][3] = ( + a[2][0] * s5 - a[2][2] * s2 + a[2][3] * s1) * rdet;

			data[2][0] = ( + a[1][0] * c4 - a[1][1] * c2 + a[1][3] * c0) * rdet;
			data[2][1] = ( - a[0][0] * c4 + a[0][1] * c2 - a[0][3] * c0) * rdet;
			data[2][2] = ( + a[3][0] * s4 - a[3][1] * s2 + a[3][3] * s0) * rdet;
			data[2][3] = ( - a[2][0] * s4 + a[2][1] * s2 - a[2][3] * s0) * rdet;

			data[3][0] = ( - a[1][0] * c3 + a[1][1] * c1 - a[1][2] * c0) * rdet;
			data[3][1] = ( + a[0][0] * c3 - a[0][1] * c1 + a[0][2] * c0) * rdet;
			data[3][2] = ( - a[3][0] * s3 + a[3][1] * s1 - a[3][2] * s0) * rdet;
			data[3][3] = ( + a[2][0] * s3 - a[2][1] * s1 + a[2][2] * s0) * rdet;
		}

		static inline Matrix4x4 Identity()
		{
			Matrix4x4 result;
			result.setIdentity();
			return result;
		}
		static inline Matrix4x4 Transpose(const Matrix4x4& matrix)
		{
			Matrix4x4 result = matrix;
			result.transpose();
			return result;
		}
		static inline Matrix4x4 Translation(float32 x, float32 y, float32 z)
		{
			Matrix4x4 result;
			result.setTranslation(x, y, z);
			return result;
		}
		static inline Matrix4x4 Translation(float32x3 vector)
		{
			Matrix4x4 result;
			result.setTranslation(vector.x, vector.y, vector.z);
			return result;
		}
		static inline Matrix4x4 Scale(float32 x, float32 y, float32 z)
		{
			Matrix4x4 result;
			result.setScale(x, y, z);
			return result;
		}
		static inline Matrix4x4 Scale(float32x3 vector)
		{
			Matrix4x4 result;
			result.setScale(vector.x, vector.y, vector.z);
			return result;
		}
		static inline Matrix4x4 RotationX(float32 angle)
		{
			Matrix4x4 result;
			result.setRotationX(angle);
			return result;
		}
		static inline Matrix4x4 RotationY(float32 angle)
		{
			Matrix4x4 result;
			result.setRotationY(angle);
			return result;
		}
		static inline Matrix4x4 RotationZ(float32 angle)
		{
			Matrix4x4 result;
			result.setRotationZ(angle);
			return result;
		}
		static inline Matrix4x4 Perspective(float32 fov, float32 aspect, float32 zNear, float32 zFar)
		{
			Matrix4x4 result;
			result.setPerspective(fov, aspect, zNear, zFar);
			return result;
		}
		static inline Matrix4x4 LookAt(const float32x3& position, const float32x3& target, const float32x3& up)
		{
			Matrix4x4 result;
			result.setLookAt(position, target, up);
			return result;
		}
		static inline Matrix4x4 LookAtCentered(const float32x3& position, const float32x3& centerdTarget, const float32x3& up)
		{
			Matrix4x4 result;
			result.setLookAtCentered(position, centerdTarget, up);
			return result;
		}
		static inline Matrix4x4 Inverse(const Matrix4x4& that)
		{
			Matrix4x4 result;
			result.setInverse(that);
			return result;
		}
	};

	inline Matrix4x4 operator * (const Matrix4x4& a, const Matrix4x4& b)
	{
		Matrix4x4 result;

		result[0][0] = a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0] + a[0][3] * b[3][0];
		result[0][1] = a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1] + a[0][3] * b[3][1];
		result[0][2] = a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2] + a[0][3] * b[3][2];
		result[0][3] = a[0][0] * b[0][3] + a[0][1] * b[1][3] + a[0][2] * b[2][3] + a[0][3] * b[3][3];

		result[1][0] = a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0] + a[1][3] * b[3][0];
		result[1][1] = a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1] + a[1][3] * b[3][1];
		result[1][2] = a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2] + a[1][3] * b[3][2];
		result[1][3] = a[1][0] * b[0][3] + a[1][1] * b[1][3] + a[1][2] * b[2][3] + a[1][3] * b[3][3];

		result[2][0] = a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0] + a[2][3] * b[3][0];
		result[2][1] = a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1] + a[2][3] * b[3][1];
		result[2][2] = a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2] + a[2][3] * b[3][2];
		result[2][3] = a[2][0] * b[0][3] + a[2][1] * b[1][3] + a[2][2] * b[2][3] + a[2][3] * b[3][3];

		result[3][0] = a[3][0] * b[0][0] + a[3][1] * b[1][0] + a[3][2] * b[2][0] + a[3][3] * b[3][0];
		result[3][1] = a[3][0] * b[0][1] + a[3][1] * b[1][1] + a[3][2] * b[2][1] + a[3][3] * b[3][1];
		result[3][2] = a[3][0] * b[0][2] + a[3][1] * b[1][2] + a[3][2] * b[2][2] + a[3][3] * b[3][2];
		result[3][3] = a[3][0] * b[0][3] + a[3][1] * b[1][3] + a[3][2] * b[2][3] + a[3][3] * b[3][3];

		return result;
	}

	inline float32x4 operator * (const float32x3& vector, const Matrix4x4& matrix)
	{
		return
		{
			vector.x * matrix[0][0] + vector.y * matrix[1][0] + vector.z * matrix[2][0] + matrix[3][0],
			vector.x * matrix[0][1] + vector.y * matrix[1][1] + vector.z * matrix[2][1] + matrix[3][1],
			vector.x * matrix[0][2] + vector.y * matrix[1][2] + vector.z * matrix[2][2] + matrix[3][2],
			vector.x * matrix[0][3] + vector.y * matrix[1][3] + vector.z * matrix[2][3] + matrix[3][3],
		};
	}
	inline float32x4 operator * (const float32x4& vector, const Matrix4x4& matrix)
	{
		return
		{
			vector.x * matrix[0][0] + vector.y * matrix[1][0] + vector.z * matrix[2][0] + vector.w * matrix[3][0],
			vector.x * matrix[0][1] + vector.y * matrix[1][1] + vector.z * matrix[2][1] + vector.w * matrix[3][1],
			vector.x * matrix[0][2] + vector.y * matrix[1][2] + vector.z * matrix[2][2] + vector.w * matrix[3][2],
			vector.x * matrix[0][3] + vector.y * matrix[1][3] + vector.z * matrix[2][3] + vector.w * matrix[3][3],
		};
	}

	inline float32x3 MultiplyBySubmatrix3x3(const float32x3& vector, const Matrix4x4& matrix)
	{
		return
		{
			vector.x * matrix[0][0] + vector.y * matrix[1][0] + vector.z * matrix[2][0],
			vector.x * matrix[0][1] + vector.y * matrix[1][1] + vector.z * matrix[2][1],
			vector.x * matrix[0][2] + vector.y * matrix[1][2] + vector.z * matrix[2][2],
		};
	}
}