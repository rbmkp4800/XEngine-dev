#pragma once

#include <XLib.h>
#include <XLib.Vectors.h>
#include <XLib.Vectors.Arithmetics.h>
#include <XLib.Vectors.Math.h>
#include <XLib.Math.Matrix4x4.h>

namespace XEngine::Render
{
	struct Camera
	{
	public:
		float32x3 position, forward, up;
		float32 fov, zNear, zFar;

		inline Camera(float32x3 position = { 0.0f, 0.0f, 0.0f },
			float32x3 forward = { 0.0f, 0.0f, 1.0f }, float32x3 up = { 0.0f, 0.0f, 1.0f },
			float32 fov = 1.0f, float32 zNear = 0.1f, float32 zFar = 1000.0f)
			: position(position), forward(forward), up(up), fov(fov), zNear(zNear), zFar(zFar) {}

		inline void translate(const float32x3& translation) { position += translation; }

		inline XLib::Matrix4x4 getViewMatrix() const { return XLib::Matrix4x4::LookAtCentered(position, forward, up); }
		inline XLib::Matrix4x4 getProjectionMatrix(float32 aspect) const { return XLib::Matrix4x4::Perspective(fov, aspect, zNear, zFar); }
	};
}