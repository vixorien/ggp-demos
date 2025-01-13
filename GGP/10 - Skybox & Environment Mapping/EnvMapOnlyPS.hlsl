
#include "ShaderStructs.hlsli"
#include "Lighting.hlsli"

#define NUM_LIGHTS 5

cbuffer ExternalData : register(b0)
{
	// Camera related
	float3 cameraPosition;

	// Material related
	float2 uvScale;
	float2 uvOffset;
}

// Texture related resources
Texture2D NormalMap			: register(t2);
TextureCube EnvironmentMap	: register(t3);

SamplerState BasicSampler	: register(s0); // Samplers use "s" registers

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	// Clean up un-normalized normals and tangents
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Adjust the uv coords
	input.uv = input.uv * uvScale + uvOffset;

	// Apply normal mapping
	input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);

	// Sample the environment map using the reflected view vector
	float3 viewVector = normalize(cameraPosition - input.worldPos);
	float3 reflectionVector = reflect(-viewVector, input.normal); // Need camera to pixel vector, so negate
	float3 reflectionColor = EnvironmentMap.Sample(BasicSampler, reflectionVector).rgb;

	// Return the environment map reflection
	return float4(reflectionColor, 1);
}