
#include "Lighting.hlsli"

cbuffer DrawData : register(b0)
{
	uint OutputWidth;
	uint OutputHeight;
	uint OutputDescriptorIndex;
}


// Specular G
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
float G1_Schlick(float Roughness, float NdotV)
{
	float k = Roughness * Roughness;
	k /= 2.0f; // Schlick-GGX version of k - Used in UE4
	
	// Staying the same
	return NdotV / (NdotV * (1.0f - k) + k);
}

// Specular G
// http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
float G_Smith(float Roughness, float NdotV, float NdotL)
{
	return G1_Schlick(Roughness, NdotV) * G1_Schlick(Roughness, NdotL);
}


// Convolves the texture cube for a particular roughness and vector
float2 IntegrateBRDF(float roughnessValue, float nDotV)
{	
	float3 V;
	V.x = sqrt(1.0f - nDotV * nDotV);
	V.y = 0;
	V.z = nDotV;

	float3 N = float3(0, 0, 1);

	float A = 0;
	float B = 0;
	
	// Run the calculation MANY times
	//  - 4096 would be an ideal number of times 
	//  - Fewer is faster, but is less accurate
	for (uint i = 0; i < MAX_IBL_SAMPLES; i++)
	{
		// Grab this sample
		float2 Xi = Hammersley2d(i, MAX_IBL_SAMPLES);
		float3 H = ImportanceSampleGGX(Xi, roughnessValue, N);
		float3 L = 2 * dot(V, H) * H - V;
		
		float nDotL = saturate(L.z);
		float nDotH = saturate(H.z);
		float vDotH = saturate(dot(V, H));
	
		// Check N dot L result
		if (nDotL > 0)
		{
			float G = G_Smith(roughnessValue, nDotV, nDotL);
			float G_Vis = G * vDotH / (nDotH * nDotV);
			float Fc = pow(1 - vDotH, 5.0f);
			
			A += (1.0f - Fc) * G_Vis;
			B += Fc * G_Vis;
		}
	}
	
	// Divide and return result
	return float2(A, B) / MAX_IBL_SAMPLES;
}


[numthreads(8, 8, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	if(id.x >= OutputWidth || id.y >= OutputHeight)
		return;
	
	// Treat the uv range (0-1) as a grid of 
	// roughness and nDotV permutations
	// Note: ROUGHNESS is Y
	//       nDotV is X
	float nDotV = id.x / (float)OutputWidth;
	float roughness = id.y / (float)OutputHeight;
	float2 brdf = IntegrateBRDF(roughness, nDotV);
	
	RWTexture2D<float4> OutputTexture = ResourceDescriptorHeap[OutputDescriptorIndex];
	OutputTexture[id.xy] = float4(brdf, 0, 1);
}