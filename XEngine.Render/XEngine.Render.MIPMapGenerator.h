#pragma once

#include <XLib.h>
#include <XLib.Vectors.h>
#include <XLib.Vectors.Arithmetics.h>

namespace XEngine::Render
{
	class MIPMapGenerator abstract final
	{
	private:
		template <typename Type> static constexpr bool checkType() { return false; }
		template <> static constexpr bool checkType<uint8>() { return true; }
		template <> static constexpr bool checkType<uint8x4>() { return true; }

		template <typename SourceType> struct getAverageType { using ResultType = void; };
		template <> struct getAverageType<uint8> { using ResultType = uint16; };
		template <> struct getAverageType<uint8x4> { using ResultType = uint16x4; };

	public:
		template <typename Type>
		static void GenerateLevel(const void* _source, uint16x2 sourceSize,
			uint32 sourceRowPitch, void* _dest, uint16x2& destSize)
		{
			Type *source = to<Type*>(_source);
			Type *dest = to<Type*>(_dest);

			static_assert(checkType<Type>(), "invalid type");

			uint16 width = sourceSize.x / 2;
			uint16 height = sourceSize.y / 2;
			using AverageType = typename getAverageType<Type>::ResultType;

			if (width == 0 && height == 0) {}
			else if (width != 0 && height == 0)
			{
				height = 1;
				for (uint32 i = 0; i < width; i++)
				{
					Type *srcLeft = source + i * 2;
					AverageType average = *srcLeft;
					Type *srcRight = srcLeft + 1;
					average += *srcRight;
					dest[i] = average >> 1;
				}
			}
			else if (width == 0 && height != 0)
			{
				width = 1;
				for (uint32 i = 0; i < width; i++)
				{
					Type *srcTop = to<Type*>(to<byte*>(source) + i * 2 * sourceRowPitch);
					AverageType average = *srcTop;
					Type *srcBottom = to<Type*>(to<byte*>(srcTop) + sourceRowPitch);
					average += *srcBottom;
					dest[i] = Type(average >> 1);
				}
			}
			else
			{
				for (uint32 i = 0; i < height; i++)
				{
					for (uint32 j = 0; j < width; j++)
					{
						Type *srcTopLeft = to<Type*>(to<byte*>(source) + i * 2 * sourceRowPitch) + j * 2;
						AverageType average = *srcTopLeft;
						Type *srcTopRight = srcTopLeft + 1;
						average += *srcTopRight;
						Type *srcBottomLeft = to<Type*>(to<byte*>(srcTopLeft) + sourceRowPitch);
						average += *srcBottomLeft;
						Type *srcBottomRight = srcBottomLeft + 1;
						average += *srcBottomRight;

						dest[i * width + j] = Type(average >> 2);
					}
				}
			}

			destSize = { width, height };
		}
	};
}