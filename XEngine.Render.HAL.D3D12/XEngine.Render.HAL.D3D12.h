#pragma once

namespace XEngine::Render::HAL
{
	class Host;

	class Device
	{
	public:
		void createBuffer();
		void createTexture();
		void createPipeline();
		void createGraphicsCommandList();
		void createComputeCommandList();
		void createCopyCommandList();

		void submitGraphics();
		void submitAsyncCompute();
		void submitCopy();
	};

	class Texture
	{

	};

	class Buffer;
	class Pipeline;

	class GraphicsCommandList
	{
	public:
		void drawNonIndexed();
		void drawIndexed();
		void drawMesh();

		void dispatch();
	};

	class ComputeCommandList
	{

	};

	class CopyCommandList
	{

	};
}
