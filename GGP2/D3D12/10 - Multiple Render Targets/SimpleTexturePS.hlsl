
cbuffer DrawData : register(b0)
{
	uint TextureIndex;	
}

// Defines the input to this pixel shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

// Sampler
SamplerState BasicSampler : register(s0);

// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
	Texture2D t = ResourceDescriptorHeap[TextureIndex];
	return t.Sample(BasicSampler, input.uv);
}