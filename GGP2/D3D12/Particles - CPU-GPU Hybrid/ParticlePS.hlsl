
// VStoPS struct for particles
struct VertexToPixel_Particle
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
	float4 colorTint	: COLOR;
};

cbuffer DrawData : register(b0)
{
	uint ParticleCBIndex;
	uint ParticleDataIndex;
	uint ParticleTextureIndex;
	uint DebugWireframe;
};

// Static sampler
SamplerState BasicSampler	: register(s0);

// Entry point for this pixel shader
float4 main(VertexToPixel_Particle input) : SV_TARGET
{
	// Sample texture and combine with input color
	Texture2D ParticleTexture = ResourceDescriptorHeap[ParticleTextureIndex];
	float4 color = ParticleTexture.Sample(BasicSampler, input.uv) * input.colorTint;
	
	// Return either particle color or white (for debugging)
	return lerp(color, float4(1, 1, 1, 0.25f), DebugWireframe);
}