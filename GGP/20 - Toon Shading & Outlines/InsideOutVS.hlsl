
// The variables defined in this cbuffer will pull their data from the 
// constant buffer (ID3D11Buffer) bound to "vertex shader constant buffer slot 0"
// It was bound using context->VSSetConstantBuffers() over in C++.
cbuffer ExternalData : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
	float outlineSize;
}

// Struct representing a single vertex worth of data
struct VertexShaderInput
{
	float3 position		: POSITION;     // XYZ position
	float2 uv			: TEXCOORD;
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
};

// Struct representing the data we're sending down the pipeline
struct VertexToPixel
{
	float4 position		: SV_POSITION;	// XYZW position (System Value Position)
};

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// --------------------------------------------------------
VertexToPixel main(VertexShaderInput input)
{
	// Set up output struct
	VertexToPixel output;

	// Get the position & normal into world space
	float3 positionWS = mul(world, float4(input.position, 1.0f)).xyz;
	float3 normalWS = normalize(mul((float3x3)world, input.normal));

	// Offset the position along its normal by the specified amount
	positionWS += normalWS * outlineSize;

	// Finalize the output position
	matrix vp = mul(projection, view);
	output.position = mul(vp, float4(positionWS, 1.0f));

	// Whatever we return will make its way through the pipeline to the
	// next programmable stage we're using (the pixel shader for now)
	return output;
}