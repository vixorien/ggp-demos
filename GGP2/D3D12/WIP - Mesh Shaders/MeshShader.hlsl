cbuffer BindlessData : register(b0)
{
	uint msVertexBufferIndex;
	uint msMeshletBufferIndex;
	uint msVertexIndicesBufferIndex;
	uint msTriangleIndicesBufferIndex;
	
	uint msPerFrameCBIndex;
	uint msPerObjectCBIndex;
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
	uint3 threadID : SV_GroupThreadID, 
	uint3 groupID : SV_GroupID, 
    out indices uint3 triangles[128], 
    out vertices VertexToPixel vertices[64])
{
	// Get bindless resources
	StructuredBuffer<Vertex> vb = ResourceDescriptorHeap[msVertexBufferIndex];
	StructuredBuffer<Meshlet> meshlets = ResourceDescriptorHeap[msMeshletBufferIndex];
	StructuredBuffer<uint> vertIndices = ResourceDescriptorHeap[msVertexIndicesBufferIndex];
	StructuredBuffer<uint> triIndices = ResourceDescriptorHeap[msTriangleIndicesBufferIndex];
	
	// Grab this meshlet for this thread group
	Meshlet m = meshlets[groupID.x];
	SetMeshOutputCounts(m.VertexCount, m.TriangleCount);
	
	// Handle indices
	if(threadID.x < m.TriangleCount)
	{
		// Grab the (packed) indices
		uint packedIndices = triIndices[m.TriangleOffset + threadID.x];
		
		// Unpack
		uint i0 = (packedIndices >> 0) & 0xFF;
		uint i1 = (packedIndices >> 8) & 0xFF;
		uint i2 = (packedIndices >> 16) & 0xFF;
		
		// Set output
		triangles[id.x] = uint3(i0, i1, i2);
	}
	
	// Handle vertex
	if(threadID.x < m.VertexCount)
	{
		ConstantBuffer<VSPerFrameData> cbFrame = ResourceDescriptorHeap[msPerFrameCBIndex];
		ConstantBuffer<VSPerObjectData> cbObject = ResourceDescriptorHeap[msPerObjectCBIndex];
		
		Vertex v = vb[vertIndices[m.VertexOffset + threadID.x]];
	
		// Set up output struct
		VertexToPixel output;

		// Calc screen position
		matrix wvp = mul(cbFrame.projection, mul(cbFrame.view, cbObject.world));
		output.screenPosition = mul(wvp, float4(v.localPosition, 1.0f));

		// Make sure the lighting vectors are in world space
		output.normal = normalize(mul((float3x3)cbObject.worldInverseTranspose, v.normal));
		output.tangent = normalize(mul((float3x3)cbObject.world, v.tangent));

		// Calc vertex world pos
		output.worldPos = mul(cbObject.world, float4(v.localPosition, 1.0f)).xyz;

		// Pass through the uv
		output.uv = v.uv;
		
		vertices[threadID.x] = output;
	}
}