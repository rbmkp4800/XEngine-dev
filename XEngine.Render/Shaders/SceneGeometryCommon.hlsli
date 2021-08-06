#define MAX_CLUSTERS_PER_SECTION 64
#define MAX_TRIANGLES_PER_CLUSTER 128
#define MAX_VERTEX_PER_CLUSTER 128

struct SceneGeometryASPayload
{
	uint clustersIndicies[MAX_CLUSTERS_PER_SECTION];
	uint clustersDataOffset; // in bytes
	uint baseTransformIndex;
	uint materialIndex;
};
