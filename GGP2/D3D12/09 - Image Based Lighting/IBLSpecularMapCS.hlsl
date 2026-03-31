
#include "Lighting.hlsli"


// External data
cbuffer DrawData : register(b0)
{
	uint OutputWidth;
	uint OutputHeight;
	uint OutputDescriptorIndex;
	uint EnvironmentMapDescriptorIndex;
	uint MipLevel;
	float Roughness;
}


// Static sampler
SamplerState BasicSampler : register(s0);


// Convolves (blurs) the texture cube for a particular roughness and reflection vector.
// This requires taking a huge number of samples for the result to look acceptable, which
// is why this is done as a pre-process rather than doing it "live".
// 
// roughness	- the roughness of the surface (rougher = blurrier)
// R			- The direction of this reflection (which is also used for the normal and view dir)
float3 ConvolveTextureCube(float roughness, float3 R)
{
	// Assume N == V == R, a common assumption to simplify the approximation quite a bit
	float3 N = R;
	float3 V = R;

	// Final color
	float3 finalColor = float3(0, 0, 0);
	float totalWeight = 0;

	// Sample the texture cube MANY times
	//  - 4096 would be an ideal number of samples 
	//  - Fewer is faster, but looks worse overall
	TextureCube EnvironmentMap = ResourceDescriptorHeap[EnvironmentMapDescriptorIndex];
	for (uint i = 0; i < MAX_IBL_SAMPLES; i++)
	{
		// Grab this sample
		float2 Xi = Hammersley2d(i, MAX_IBL_SAMPLES);
		float3 H = ImportanceSampleGGX(Xi, roughness, N);
		float3 L = 2 * dot(V, H) * H - V;

		// Check N dot L result
		float nDotL = saturate(dot(N, L));
		if (nDotL > 0)
		{
			float3 thisColor = (EnvironmentMap.SampleLevel(BasicSampler, L, 0).rgb);
			thisColor = pow(abs(thisColor), 2.2f);  // Using abs() to stop a compiler warning
			finalColor += thisColor * nDotL;
			totalWeight += nDotL;
		}
	}

	// Divide and return result
	return finalColor / totalWeight; // Note: Not gamma correcting here, as this will be used as-is later
}



[numthreads(8, 8, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	// Get -1 to 1 range on x/y
	float2 o = id.xy / float2(OutputWidth, OutputHeight) * 2 - 1;
	
	// Figure out the z ("normal" of this pixel)
	// based on id.z, which should range [0,5], 
	// one id per cube map face
	float3 zDir;
	switch (id.z)
	{
		default:
		case 0: zDir = float3(+1, -o.y, -o.x); break;
		case 1:	zDir = float3(-1, -o.y, +o.x); break;
		case 2:	zDir = float3(+o.x, +1, +o.y); break;
		case 3:	zDir = float3(+o.x, -1, -o.y); break;
		case 4: zDir = float3(+o.x, -o.y, +1); break;
		case 5: zDir = float3(-o.x, -o.y, -1); break;
	}
	zDir = normalize(zDir);

	// Process the convolution for the direction of this pixel
	float3 c = ConvolveTextureCube(Roughness, zDir);
	RWTexture2DArray<float4> OutputTexture = ResourceDescriptorHeap[OutputDescriptorIndex];
	OutputTexture[id] = float4(c, 1);
}