
cbuffer ExternalData : register(b0)
{
	uint vsVertexBufferIndex;
	uint vsCBIndex;
	uint psSkyboxIndex;
}

// Texture-related resources
SamplerState BasicSampler	: register(s0);

struct VertexToPixel_Sky
{
	float4 screenPosition	: SV_POSITION;
	float3 sampleDir		: DIRECTION;
};

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel_Sky input) : SV_TARGET
{
	// When we sample a TextureCube (like "skyTexture"), we need
	// to provide a direction in 3D space (a float3) instead of a uv coord
	TextureCube SkyTexture = ResourceDescriptorHeap[psSkyboxIndex];
	return SkyTexture.Sample(BasicSampler, input.sampleDir);
}