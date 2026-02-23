
// 2D value to 1D "random" value
float random(float2 uv)
{
	return frac(sin(dot(uv, float2(12.3456, 67.8912))) * 12346.6789);
}

// 2D value to 2D "random" vector
float2 random2D(float2 uv)
{
	return frac(sin(mul(uv, float2x2(0.129898, 0.78233, 0.81314, 0.15926))) * 12346.6789);
}

// 3D value to 3D "random" vector
float3 random3D(float3 uv)
{
	return frac(sin(mul(uv, float3x3(0.129898, 0.78233, 0.81314, 0.15926, 0.54321, 0.98765, 0.90210, 0.8675309, 0.6789))) * 12346.6789);
}

// 2D value to 2D "random" unit vector
float2 randomUnitVector2D(float2 uv)
{
	return normalize(random2D(uv) - 0.5f);	
}

// 3D value to 3D "random" unit vector
float3 randomUnitVector3D(float3 uv)
{
	return normalize(random3D(uv) - 0.5f);	
}

// Basic implementation of 2D perlin noise
float perlin2D(float2 uv)
{
	float2 i = floor(uv); // Int part
	float2 fr = frac(uv); // Fractional part
	
	float2 aVec = randomUnitVector2D(i + float2(0, 0));
	float2 bVec = randomUnitVector2D(i + float2(1, 0));
	float2 cVec = randomUnitVector2D(i + float2(0, 1));
	float2 dVec = randomUnitVector2D(i + float2(1, 1));
	
	float a = dot(aVec, float2(0, 0) - fr);
	float b = dot(bVec, float2(1, 0) - fr);
	float c = dot(cVec, float2(0, 1) - fr);
	float d = dot(dVec, float2(1, 1) - fr);
	
	// Perlin's quintic "smoother step": 6x^5 - 15x^4 + 10x^3
	float2 q = fr * fr * fr * (fr * (6.0 * fr - 15.0f) + 10.0f);

	// Interpolate a<->b and c<->d (the "x" axis)
	float xtop = lerp(a, b, q.x);
	float xbot = lerp(c, d, q.x);
	
	// Interpolate "y" axis, then normalize range to approx 0-1
	return lerp(xtop, xbot, q.y) * 0.7f + 0.5f;
}

// Basic implementation of 3D perlin noise
float perlin3D(float3 uv)
{
	float3 i = floor(uv); // Int part
	float3 fr = frac(uv);  // Fractional part
	
	float3 aVec = randomUnitVector3D(i + float3(0, 0, 0));
	float3 bVec = randomUnitVector3D(i + float3(1, 0, 0));
	float3 cVec = randomUnitVector3D(i + float3(0, 1, 0));
	float3 dVec = randomUnitVector3D(i + float3(1, 1, 0));
	float3 eVec = randomUnitVector3D(i + float3(0, 0, 1));
	float3 fVec = randomUnitVector3D(i + float3(1, 0, 1));
	float3 gVec = randomUnitVector3D(i + float3(0, 1, 1));
	float3 hVec = randomUnitVector3D(i + float3(1, 1, 1));
	
	float a = dot(aVec, float3(0, 0, 0) - fr);
	float b = dot(bVec, float3(1, 0, 0) - fr);
	float c = dot(cVec, float3(0, 1, 0) - fr);
	float d = dot(dVec, float3(1, 1, 0) - fr);
	float e = dot(eVec, float3(0, 0, 1) - fr);
	float f = dot(fVec, float3(1, 0, 1) - fr);
	float g = dot(gVec, float3(0, 1, 1) - fr);
	float h = dot(hVec, float3(1, 1, 1) - fr);
	
	// Perlin's quintic "smoother step": 6x^5 - 15x^4 + 10x^3
	float3 q = fr * fr * fr * (fr * (6.0 * fr - 15.0f) + 10.0f);

	// Interpolate each "x" axis
	float xtop_z0 = lerp(a, b, q.x);
	float xbot_z0 = lerp(c, d, q.x);
	float xtop_z1 = lerp(e, f, q.x);
	float xbot_z1 = lerp(g, h, q.x);
	
	// Interpolate each "y" axis
	float y_z0 = lerp(xtop_z0, xbot_z0, q.y);
	float y_z1 = lerp(xtop_z1, xbot_z1, q.y);
	
	// Interpolate "z" axis, then normalize range to approx 0-1
	return lerp(y_z0, y_z1, q.z) * 0.7f + 0.5f;
}

// Layering several iterations of 2D perlin noise on top of each other
float layeredPerlin2D(float2 uv, uint layers)
{
	float frequency = 2.0f;
	float amplitude = 1;
	float totalAmplitude = 0;
	
	float noise = 0;
	for (uint i = 0; i < layers; i++)
	{
		uv *= frequency;

		noise += perlin2D(uv) * amplitude;
		totalAmplitude += amplitude;
		amplitude *= 0.5f;
		frequency *= 1.1f;
	}
	
	return noise / totalAmplitude;
}

// Layering several iterations of 3D perlin noise on top of each other
float layeredPerlin3D(float3 uv, uint layers)
{
	float frequency = 2.0f;
	float amplitude = 1;
	float totalAmplitude = 0;
	
	float noise = 0;
	for (uint i = 0; i < layers; i++)
	{
		uv *= frequency;

		noise += perlin3D(uv) * amplitude;
		totalAmplitude += amplitude;
		amplitude *= 0.5f;
		frequency *= 1.1f;
	}
	
	return noise / totalAmplitude;
}

// === Shader itself starts here ===

cbuffer DrawIndices : register(b0)
{
	uint albedoIndex;
	uint normalIndex;
	uint roughIndex;
	uint metalIndex;
	uint noiseIndex;
	float time;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	// Basic controls
	const uint layers = 5;
	const float speed = 0.02f;
	
	// Calculate proper uv, using time as Z value
	float3 uvAndTime = float3(threadID.xy / 512.0f, time * speed);
	
	// Calculate neighbor uvs, too
	float neighbor = 1.0f / 512.0f;
	float3 uvRight = uvAndTime;
	float3 uvDown = uvAndTime;
	uvRight.x += neighbor;
	uvDown.y += neighbor;
	
	// Run noise calculations
	float noise = layeredPerlin3D(uvAndTime, layers);
	float noiseR = layeredPerlin3D(uvRight, layers);
	float noiseD = layeredPerlin3D(uvDown, layers);
	
	// Place the result in the output textures
	RWTexture2D<unorm float4> AlbedoTexture = ResourceDescriptorHeap[albedoIndex];
	RWTexture2D<unorm float4> NormalTexture = ResourceDescriptorHeap[normalIndex];
	RWTexture2D<unorm float4> RoughTexture = ResourceDescriptorHeap[roughIndex];
	RWTexture2D<unorm float4> MetalTexture = ResourceDescriptorHeap[metalIndex];
	RWTexture2D<unorm float4> NoiseTexture = ResourceDescriptorHeap[noiseIndex];
	
	float metal = 1 - round(noise);
	float rough = metal == 0.0f ? 1 - (noise * 5.0f - 2.5f) : 0.1f;
	float3 albedo = metal == 0.0f ? float3(0.75f, 0.75f, 0.75f) : 1;//float3(0.2f, 0.4f, 0.9f);

	float normalScale = 50.0f * (1 - (noise * 5.0f - 2.5f));
	float normX = metal == 0.0f ? noiseR - noise : round(noiseR) - round(noise);
	float normY = metal == 0.0f ? noiseD - noise : round(noiseD) - round(noise);
	float3 normal = normalize(float3(normX * normalScale, normY * normalScale, 1)) * 0.5f + 0.5f; // Packed
	
	AlbedoTexture[threadID.xy] = float4(albedo, 1);
	NormalTexture[threadID.xy] = float4(normal, 1);
	RoughTexture[threadID.xy] = float4(rough.rrr, 1);
	MetalTexture[threadID.xy] = float4(metal.rrr, 1);
	NoiseTexture[threadID.xy] = float4(noise.rrr, 1);
	
}