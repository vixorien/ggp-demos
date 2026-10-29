

cbuffer ExternalData : register(b0)
{
	uint vsVertexBufferIndex;
	uint vsCBIndex;
	uint psSkyboxIndex;
}

struct VSPerFrameData
{
	matrix view;
	matrix projection;
};

struct Vertex
{
	float3 localPosition	: POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
};

struct VertexToPixel_Sky
{
	float4 screenPosition	: SV_POSITION;
	float3 sampleDir		: DIRECTION;
};


// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// --------------------------------------------------------
VertexToPixel_Sky main(uint vertexID : SV_VertexID)
{
	ConstantBuffer<VSPerFrameData> cb = ResourceDescriptorHeap[vsCBIndex];
	StructuredBuffer<Vertex> vb = ResourceDescriptorHeap[vsVertexBufferIndex];
	Vertex vert = vb[vertexID];
	
	// Set up output struct
	VertexToPixel_Sky output;

	// Modify the view matrix and remove the translation portion
	matrix viewNoTranslation = cb.view;
	viewNoTranslation._14 = 0;
	viewNoTranslation._24 = 0;
	viewNoTranslation._34 = 0;

	// Multiply the view (without translation) and the projection
	matrix vp = mul(cb.projection, viewNoTranslation);
	output.screenPosition = mul(vp, float4(vert.localPosition, 1.0f));

	// For the sky vertex to be ON the far clip plane
	// (a.k.a. as far away as possible but still visible),
	// we can simply set the Z = W, since the xyz will 
	// automatically be divided by W in the rasterizer
	output.screenPosition.z = output.screenPosition.w;

	// Use the vert's position as the sample direction for the cube map!
	output.sampleDir = vert.localPosition;

	// Whatever we return will make its way through the pipeline to the
	// next programmable stage we're using (the pixel shader for now)
	return output;
}