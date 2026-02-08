cbuffer BindlessData : register(b0)
{
	uint vsVertexBufferIndex;
	uint vsPerFrameCBIndex;
	uint vsPerObjectCBIndex;
	uint psPerFrameCBIndex;
	uint psPerObjectCBIndex;
}

struct VSPerFrameData
{
	matrix view;
	matrix projection;
};

struct VSPerObjectData
{
	matrix world;
	matrix worldInverseTranspose;
};

// Struct representing a single vertex worth of data
struct Vertex
{
	float3 localPosition	: POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
};

// Struct representing the data we're sending down the pipeline
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION;
};

struct Meshlet {
	uint VertexOffset;
	uint TriangleOffset;
	uint VertexCount;
	uint TriangleCount;
};

[outputtopology("triangle")]
[numthreads(128, 1, 1)]
void main(
	uint3 id : SV_GroupThreadID, 
	uint3 groupID : SV_GroupID, 
    out indices uint3 triangles[128], 
    out vertices VertexToPixel vertices[64])
{
	SetMeshOutputCounts(1,1);
	
	vertices[id.x].screenPosition = 0;
}