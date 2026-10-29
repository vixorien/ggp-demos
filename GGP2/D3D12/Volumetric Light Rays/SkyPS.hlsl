
cbuffer ExternalData : register(b0)
{
	uint vsVertexBufferIndex;
	uint vsCBIndex;
	uint psSkyboxIndex;
	
	uint useSkyboxColor;

	float3 sunDirection;
	float falloffExponent;
	
	float3 sunColor;
}

// Texture-related resources
SamplerState BasicSampler	: register(s0);

struct VertexToPixel_Sky
{
	float4 screenPosition	: SV_POSITION;
	float3 sampleDir		: DIRECTION;
};

struct PS_Output
{
	float4 sceneColor	: SV_TARGET0;
	float4 sunAndOccluders	: SV_TARGET1;
};

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
PS_Output main(VertexToPixel_Sky input)
{
	// Calculate the falloff from the sun direction
	float falloff = saturate(dot(normalize(sunDirection), normalize(input.sampleDir)));
	falloff = pow(falloff, falloffExponent);

	// Colors
	TextureCube SkyTexture = ResourceDescriptorHeap[psSkyboxIndex];
	float3 skyColor = pow(SkyTexture.Sample(BasicSampler, input.sampleDir).rgb, 2.2f);
	float3 lightColor = lerp(sunColor, skyColor, useSkyboxColor);

	// Output
	PS_Output output;
	output.sceneColor = float4(skyColor, 1);
	output.sunAndOccluders = float4(falloff * lightColor, 1);
	return output;
}