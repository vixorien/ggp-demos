
cbuffer DrawData : register(b0)
{
	uint SceneColorIndex;	
	uint SunOcclusionIndex;
	
	float2 lightPosScreenSpace;
	int numSamples;
	float density;
	float weight;
	float decay;
	float exposure;
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

	
	// Adjust light pos to UV space
	float2 lightPosUV = lightPosScreenSpace * 0.5f + 0.5f;
	lightPosUV.y = 1.0f - lightPosUV.y;

	// Start with initial color sample from sky/occlusion buffer
	Texture2D SunOcclusion = ResourceDescriptorHeap[SunOcclusionIndex];
	float3 rayColor = SunOcclusion.Sample(BasicSampler, input.uv).rgb;

	// Set up the ray for the screen space ray march
	float2 rayPos = input.uv;
	float2 rayDir = rayPos - lightPosUV;
	rayDir *= 1.0f / numSamples * density;

	// Light decays as we get further from light source
	float illumDecay = 1.0f;

	// Loop across screen and accumulate light
	for (int i = 0; i < numSamples; i++)
	{
		// Step and grab new sample and applu attenuation
		rayPos -= rayDir;
		float3 stepColor = SunOcclusion.Sample(BasicSampler, rayPos).rgb;
		stepColor *= illumDecay * weight;

		// Accumulate color as we go
		rayColor += stepColor;

		// Exponential decay
		illumDecay *= decay;
	}

	// Combine (using overall exposure for ray color), gamma correct, return
	Texture2D SceneColors = ResourceDescriptorHeap[SceneColorIndex];
	float3 finalSceneColor = SceneColors.Sample(BasicSampler, input.uv).rgb;
	return float4(pow(rayColor * exposure + finalSceneColor, 1.0f / 2.2f), 1);
}