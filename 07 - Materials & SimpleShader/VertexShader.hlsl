

cbuffer ExternalData : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
}

// Struct representing a single vertex worth of data
struct VertexShaderInput
{
	float3 localPosition	: POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
};

// Struct representing the data we're sending down the pipeline
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
};

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// --------------------------------------------------------
VertexToPixel main(VertexShaderInput input)
{
	// Set up output struct
	VertexToPixel output;

	// Calculate screen position of this vertex
	matrix wvp = mul(projection, mul(view, world));
	output.screenPosition = mul(wvp, float4(input.localPosition, 1.0f));

	// Pass other data through (for now)
	output.uv = input.uv;
	output.normal = input.normal; // This will need to be transformed for proper lighting

	return output;
}