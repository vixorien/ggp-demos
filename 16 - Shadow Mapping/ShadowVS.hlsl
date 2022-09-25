
#include "ShaderStructs.hlsli"

// Constant Buffer for external (C++) data
cbuffer externalData : register(b0)
{
	matrix world;
	matrix view;
	matrix projection;
};

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// --------------------------------------------------------
VertexToPixel_Shadow main(VertexShaderInput input)
{
	// Set up output
	VertexToPixel_Shadow output;

	// Calculate output position
	matrix wvp = mul(projection, mul(view, world));
	output.screenPosition = mul(wvp, float4(input.localPosition, 1.0f));

	return output;
}