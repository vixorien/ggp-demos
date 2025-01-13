
#include "ShaderStructs.hlsli"


cbuffer ExternalData : register(b0)
{
	matrix world;
	matrix worldInvTrans;
	matrix view;
	matrix projection;
}


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
	output.normal = normalize(mul((float3x3)worldInvTrans, input.normal));
	output.tangent = normalize(mul((float3x3)worldInvTrans, input.tangent));
	output.worldPos = mul(world, float4(input.localPosition, 1.0f)).xyz;

	return output;
}