#pragma once

#include "XLib.h"
#include "XLib.Vectors.h"
#include "XLib.Math.h"

// TODO: implement *=

namespace XLib
{
	struct Matrix2x3
	{
		float32 data[2][3];

		inline float32* operator [] (uint32 row) { return data[row]; }
		inline const float32* operator [] (uint32 row) const { return data[row]; }

		inline void clear()
		{
			data[0][0] = 0.0f;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = 0.0f;
			data[1][2] = 0.0f;
		}
		inline void identity()
		{
			data[0][0] = 1.0f;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = 1.0f;
			data[1][2] = 0.0f;
		}
		inline void translation(float32 x, float32 y)
		{
			data[0][0] = 1.0f;
			data[0][1] = 0.0f;
			data[0][2] = x;

			data[1][0] = 0.0f;
			data[1][1] = 1.0f;
			data[1][2] = y;
		}
		inline void scale(float32 scale)
		{
			data[0][0] = scale;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = scale;
			data[1][2] = 0.0f;
		}
		inline void scale(float32 xscale, float32 yscale)
		{
			data[0][0] = xscale;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = yscale;
			data[1][2] = 0.0f;
		}
		inline void scale(float32 xscale, float32 yscale, float32 x, float32 y)
		{
			data[0][0] = xscale;
			data[0][1] = 0.0f;
			data[0][2] = x * (1.0f - xscale);

			data[1][0] = 0.0f;
			data[1][1] = yscale;
			data[1][2] = y * (1.0f - yscale);
		}
		inline void rotation(float32 angle)
		{
			float32 cos = Math::Cos(angle);
			float32 sin = Math::Sin(angle);

			data[0][0] = cos;
			data[0][1] = sin;
			data[0][2] = 0.0f;

			data[1][0] = -sin;
			data[1][1] = cos;
			data[1][2] = 0.0f;
		}
		// TODO: check
		/*inline void rotation(float32 angle, float32 x, float32 y)
		{
			float32 cos = Math::Cos(angle);
			float32 sin = Math::Sin(angle);

			data[0][0] = cos;
			data[0][1] = sin;
			data[0][2] = x * (1.0f - cos) - sin * y;

			data[1][0] = -sin;
			data[1][1] = cos;
			data[1][2] = y * (1.0f - cos) + sin * x;
		}*/
		inline void verticalReflection()
		{
			data[0][0] = 1.0f;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = -1.0f;
			data[1][2] = 0.0f;
		}
		inline void horizontalReflection()
		{
			data[0][0] = -1.0f;
			data[0][1] = 0.0f;
			data[0][2] = 0.0f;

			data[1][0] = 0.0f;
			data[1][1] = 1.0f;
			data[1][2] = 0.0f;
		}

		// TODO: implement
		/*inline void reflection(float32 x, float32 y)
		{
			float32 d = sqr(x) + sqr(y);
			float32 m = 2.0f * x * y / d;
			float32 s = (sqr(x) - sqr(y)) / d;

			data[0][0] = s;
			data[0][1] = m;
			data[1][0] = m;
			data[1][1] = -s;
			data[2][0] = 0.0f;
			data[2][1] = 0.0f;
		}*/

		inline void translation(float32x2 translationVector) { translation(translationVector.x, translationVector.y); }
		inline void scale(float32x2 scaleVector) { scale(scaleVector.x, scaleVector.y); }
		inline void scale(float32x2 scaleVector, float32x2 positionVector) { scale(scaleVector.x, scaleVector.y, positionVector.x, positionVector.y); }
		//inline void rotation(float32 angle, float32x2 positionVector) { rotation(angle, positionVector.x, positionVector.y); }
		//inline void reflection(float32x2 reflectionVector) { reflection(reflectionVector.x, reflectionVector.y); }

		static inline Matrix2x3 Identity()
		{
			Matrix2x3 result;
			result.identity();
			return result;
		}
		static inline Matrix2x3 Translation(float32 x, float32 y)
		{
			Matrix2x3 result;
			result.translation(x, y);
			return result;
		}
		static inline Matrix2x3 Translation(float32x2 translationVector)
		{
			Matrix2x3 result;
			result.translation(translationVector);
			return result;
		}
		static inline Matrix2x3 Scale(float32 scale)
		{
			Matrix2x3 matrix;
			matrix.scale(scale);
			return matrix;
		}
		static inline Matrix2x3 Scale(float32 x, float32 y)
		{
			Matrix2x3 matrix;
			matrix.scale(x, y);
			return matrix;
		}
		static inline Matrix2x3 Scale(float32x2 scaleVector)
		{
			Matrix2x3 matrix;
			matrix.scale(scaleVector);
			return matrix;
		}
		static inline Matrix2x3 Rotation(float32 angle)
		{
			Matrix2x3 matrix;
			matrix.rotation(angle);
			return matrix;
		}
		/*static inline Matrix2x3 Rotation(float32 angle, float32 x, float32 y)
		{
			Matrix2x3 matrix;
			matrix.rotation(angle, x, y);
			return matrix;
		}*/
		/*static inline Matrix2x3 Rotation(float32 angle, float32x2 position)
		{
			Matrix2x3 matrix;
			matrix.rotation(angle, position);
			return matrix;
		}*/
		static inline Matrix2x3 VerticalReflection()
		{
			Matrix2x3 matrix;
			matrix.verticalReflection();
			return matrix;
		}
		static inline Matrix2x3 HorizontalReflection()
		{
			Matrix2x3 matrix;
			matrix.horizontalReflection();
			return matrix;
		}
		/*static inline Matrix2x3 Reflection(float32 x, float32 y)
		{
			Matrix2x3 matrix;
			matrix.reflection(x, y);
			return matrix;
		}*/
		/*static inline Matrix2x3 Reflection(float32x2 reflectionVector)
		{
			Matrix2x3 matrix;
			matrix.reflection(reflectionVector);
			return matrix;
		}*/
	};

	inline Matrix2x3 operator * (const Matrix2x3& a, const Matrix2x3& b)
	{
		Matrix2x3 result;

		result[0][0] = a[0][0] * b[0][0] + a[0][1] * b[1][0];
		result[0][1] = a[0][0] * b[0][1] + a[0][1] * b[1][1];
		result[0][2] = a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2];

		result[1][0] = a[1][0] * b[0][0] + a[1][1] * b[1][0];
		result[1][1] = a[1][0] * b[0][1] + a[1][1] * b[1][1];
		result[1][2] = a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2];

		return result;
	}

	// TODO: refactor order
	inline float32x2 operator * (const float32x2& vector, const Matrix2x3& matrix)
	{
		return
		{
			matrix[0][0] * vector.x + matrix[0][1] * vector.y + matrix[0][2],
			matrix[1][0] * vector.x + matrix[1][1] * vector.y + matrix[1][2],
		};
	}
}