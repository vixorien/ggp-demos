

cbuffer DrawIndices : register(b0)
{
	uint noiseIndex;
	uint albedoIndex;
	uint normalIndex;
	uint roughIndex;
	uint metalIndex;
	float time;
}

SamplerState WrapSampler : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	Texture2D NoiseTexture = ResourceDescriptorHeap[noiseIndex];
	
	float2 uv = threadID.xy / 512.0f;

	// Calculate neighbor uvs, too
	float neighbor = 1.0f / 512.0f;
	float2 uvRight = uv;
	float2 uvDown = uv;
	uvRight.x += neighbor;
	uvDown.y += neighbor;
	
	// Run noise calculations
	float noise = NoiseTexture.Sample(WrapSampler, uv).r;
	float noiseX = NoiseTexture.Sample(WrapSampler, uvRight).r;
	float noiseY = NoiseTexture.Sample(WrapSampler, uvDown).r;
	float xDiff = noiseX - noise;
	float yDiff = noiseY - noise;
	
	// Metal is either 0 or 1, with a slight gradient between
	float metal = 1 - saturate(noise * 100 - 50);
	
	// Calculate a roughness by "following" the perlin noise
	float weirdRough = 0;
	float noise0 = noise;
	for(int i = 0; i < 3; i++)
	{
		uv.xy += (noise0 * 2 - 1) * 0.05f;
		float noise1 = NoiseTexture.Sample(WrapSampler, uv).r;
		
		weirdRough += abs(noise0 - noise1) * 5.0f;
		noise0 = noise1;
	}
	
	// Roughness is either inverted noise or "weird" depending on metalness
	float rough = lerp(1 - (noise * 0.5f + 0.5f), weirdRough, metal);
	
	
	
	// Albedo is dark red for non-metal, and brighter red for metal
	float3 albedo = lerp(float3(1, 0.1f, 0.1f) * noise, float3(1,0.75f,0.75f), metal);

	// Normal is based on the difference of noise at this pixel and its X/Y neighbors
	float normalScale = 50.0f * ((noise * 5.0f - 2.5f));
	float normX = lerp(xDiff * normalScale, 0, metal);
	float normY = lerp(yDiff * normalScale, 0, metal);
	float3 normal = normalize(float3(normX, normY, 1)) * 0.5f + 0.5f; // Packed
	
	
	// Place the result in the output textures
	RWTexture2D<unorm float4> AlbedoTexture = ResourceDescriptorHeap[albedoIndex];
	RWTexture2D<unorm float4> NormalTexture = ResourceDescriptorHeap[normalIndex];
	RWTexture2D<unorm float4> RoughTexture = ResourceDescriptorHeap[roughIndex];
	RWTexture2D<unorm float4> MetalTexture = ResourceDescriptorHeap[metalIndex];
	//RWTexture2D<unorm float4> NoiseTexture = ResourceDescriptorHeap[noiseIndex];
	
	AlbedoTexture[threadID.xy] = float4(albedo, 1);
	NormalTexture[threadID.xy] = float4(normal, 1);
	RoughTexture[threadID.xy] = float4(rough.rrr, 1);
	MetalTexture[threadID.xy] = float4(metal.rrr, 1);
	//NoiseTexture[threadID.xy] = float4(noise.rrr, 1);
	
}

//////////////