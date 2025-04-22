

#include "ShaderStructs.hlsli"
#include "Lighting.hlsli"



cbuffer ExternalData : register(b0)
{
	// Scene related
	Light lights[MAX_LIGHTS];
	int lightCount;
	
	// Camera related
	float3 cameraPosition;
	
	// Material
	float2 uvScale;
	float2 uvOffset;

	// Refraction details
	float screenWidth;
	float screenHeight;
	float refractionScale;
	int useRefractionSilhouette;
}

// Texture related resources
Texture2D NormalMap				: register(t0);

// Environment map for reflections
TextureCube EnvironmentMap		: register(t1);

// Refraction requirement
Texture2D ScreenPixels			: register(t2);
Texture2D RefractionSilhouette	: register(t3);

// Samplers
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);

// Fresnel term - Schlick approx.
float SimpleFresnel(float3 n, float3 v, float f0)
{
	// Pre-calculations
	float NdotV = saturate(dot(n, v));

	// Final value
	return f0 + (1 - f0) * pow(1 - NdotV, 5);
}


float4 main(VertexToPixel input) : SV_TARGET
{
	// Clean up un-normalized normals
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Adjust uv scaling
	input.uv = input.uv * uvScale + uvOffset;

	// Use normal mapping
	input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);

	// Using normal map as "displacement"
	float2 offsetUV = NormalMap.Sample(BasicSampler, input.uv).xy * 2 - 1;
	offsetUV.y *= -1;
	
	// Calculate screen UV
	float2 screenUV = input.screenPosition.xy / float2(screenWidth, screenHeight);
	
	// Final refracted UV (where to pull the pixel color)
	// If not using refraction at all, just use the screen UV itself
	float2 refractedUV = screenUV + offsetUV * refractionScale;

	// Get the depth at the offset and verify its valid
	float silhouette = RefractionSilhouette.Sample(ClampSampler, refractedUV).r;
	if (useRefractionSilhouette && silhouette < 1)
	{
		// Invalid spot for the offset so default to THIS pixel's UV for the "refraction"
		refractedUV = screenUV;
	}

	// Get the color at the (now verified) offset UV
	float3 sceneColor = pow(ScreenPixels.Sample(ClampSampler, refractedUV).rgb, 2.2f); // Un-gamma correct

	// Get reflections
	float3 viewToCam = normalize(cameraPosition - input.worldPos);
	float3 viewRefl = normalize(reflect(-viewToCam, input.normal));
	float3 envSample = EnvironmentMap.Sample(BasicSampler, viewRefl).rgb;

	// Determine the reflectivity based on viewing angle
	// using the Schlick approximation of the Fresnel term
	float fresnel = SimpleFresnel(input.normal, viewToCam, F0_NON_METAL);
	return float4(pow(lerp(sceneColor, envSample, fresnel), 1.0f / 2.2f), 1); // Re-gamma correct after linear interpolation
}