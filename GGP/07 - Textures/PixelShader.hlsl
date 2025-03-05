
#include "ShaderStructs.hlsli"


cbuffer ExternalData : register(b0)
{
	// Material related
	float3 colorTint;
	float2 uvScale;
	float2 uvOffset;
}

// Texture related resources
Texture2D SurfaceTexture	: register(t0); // Textures use "t" registers

SamplerState BasicSampler	: register(s0); // Samplers use "s" registers

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	// Adjust the uv coords
	input.uv = input.uv * uvScale + uvOffset;

	// Sample the texture and tint for the final surface color
	float3 surfaceColor = SurfaceTexture.Sample(BasicSampler, input.uv).rgb;
	surfaceColor *= colorTint;

	// Should have the complete light contribution at this point
	return float4(surfaceColor, 1);
}