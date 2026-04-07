

#include "Lighting.hlsli"


cbuffer DrawData : register(b0)
{
	uint OutputWidth;
	uint OutputHeight;
	uint OutputDescriptorIndex;
	uint EnvironmentMapDescriptorIndex;
}

// Static sampler
SamplerState BasicSampler : register(s0);

// http://www.codinglabs.net/article_physically_based_rendering.aspx
[numthreads(8, 8, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	// Get -1 to 1 range on x/y
	float2 o = id.xy / float2(OutputWidth, OutputHeight) * 2 - 1;
	
	// Figure out the z ("normal" of this pixel)
	// based on id.z, which should range [0,5], 
	// one id per cube map face
	float3 xDir, yDir, zDir;
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

	// Calculate the tangent basis
	float3 yAxis = float3(0,1,0);
	float3 xAxis = float3(1,0,0);
	
	// If Z is pointing at the Y axis, a cross product
	// won't work right, so use X instead
	if(dot(zDir, yAxis) > 0.99f)
		xDir = normalize(cross(xAxis, zDir));
	else
		xDir = normalize(cross(yAxis, zDir));
	
	yDir = normalize(cross(zDir, xDir));
	
	// Total color (to be averaged at the end)
	float3 totalColor = float3(0, 0, 0);
	int sampleCount = 0;

	// Variables for various sin/cos values
	float sinT, cosT, sinP, cosP;

	// Loop around the hemisphere (360 degrees)
	TextureCube EnvironmentMap = ResourceDescriptorHeap[EnvironmentMapDescriptorIndex];
	for (float phi = 0.0f; phi < TWO_PI; phi += IRRADIANCE_SAMPLE_STEP_PHI)
	{
		// Grab the sin and cos of phi
		sincos(phi, sinP, cosP);

		// Loop down the hemisphere (90 degrees)
		for (float theta = 0.0f; theta < HALF_PI; theta += IRRADIANCE_SAMPLE_STEP_THETA)
		{
			// Get the sin and cos of theta
			sincos(theta, sinT, cosT);

			// Get an X/Y/Z direction from the polar coords
			float3 hemisphereDir = float3(sinT * cosP, sinT * sinP, cosT);

			// Change to world space based on this pixel's direction
			hemisphereDir =
				hemisphereDir.x * xDir +
				hemisphereDir.y * yDir +
				hemisphereDir.z * zDir;

			// Sample in that direction
			totalColor += cosT * sinT * pow(abs(EnvironmentMap.Sample(BasicSampler, hemisphereDir).rgb), 2.2f);
			sampleCount++;
		}
	}

	// Note: Not gamma correcting here, as this will be used as-is later
	float3 finalColor = PI * totalColor / sampleCount;
	RWTexture2DArray<float4> OutputTexture = ResourceDescriptorHeap[OutputDescriptorIndex];
	OutputTexture[id] = float4(finalColor, 1);
}
