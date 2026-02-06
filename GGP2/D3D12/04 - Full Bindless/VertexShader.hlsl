
cbuffer BindlessData : register(b0)
{
	uint vsVertexBufferIndex;
	uint vsConstantBufferIndex;	
	uint psConstantBufferIndex;	
}

struct VSConstantBufferData
{
	matrix world;
	matrix worldInverseTranspose;
	matrix view;
	matrix projection;
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

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// --------------------------------------------------------
VertexToPixel main(uint vertexID : SV_VertexID)
{
	ConstantBuffer<VSConstantBufferData> cb = ResourceDescriptorHeap[vsConstantBufferIndex];
	StructuredBuffer<Vertex> vb = ResourceDescriptorHeap[vsVertexBufferIndex];
	Vertex vert = vb[vertexID];
	
	// Set up output struct
	VertexToPixel output;

	// Calc screen position
	matrix wvp = mul(cb.projection, mul(cb.view, cb.world));
	output.screenPosition = mul(wvp, float4(vert.localPosition, 1.0f));

	// Make sure the lighting vectors are in world space
	output.normal = normalize(mul((float3x3)cb.worldInverseTranspose, vert.normal));
	output.tangent = normalize(mul((float3x3)cb.world, vert.tangent));

	// Calc vertex world pos
	output.worldPos = mul(cb.world, float4(vert.localPosition, 1.0f)).xyz;

	// Pass through the uv
	output.uv = vert.uv;

	return output;
}