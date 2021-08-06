#pragma once

#include <XLib.h>

namespace XEngine::Render
{
	// Device resources
	using BufferAddress = uint64;
	using TextureHandle = uint16;
	using MaterialShaderHandle = uint16;
	using MaterialInstanceHandle = uint32; // Also encodes `MaterialShaderHandle`
	using GeometrySectionBundleHandle = uint32;

	//struct GeometrySectionBundleHierarchyNode
	struct GeometrySectionDesc
	{
		// uint16 clustersBufferIdx;
		// uint32 clustersOffset;
		BufferAddress clustersAddress; // temp
		uint16 transformsOffset;
		//uint8 materialIndex;
		MaterialInstanceHandle material; // temp
		uint8 clusterCount;

		// Bounding volume data
		// LOD data
	};


	// Scene resources
	//using SceneViewHandle = uint16;
	using TransformAddress = uint32;
	//using InplaceGeometrySectionsInstanceHandle = uint32;
	using GeometrySectionBundleInstanceHandle = uint32;
	//using LightHandle = uint32;
}
