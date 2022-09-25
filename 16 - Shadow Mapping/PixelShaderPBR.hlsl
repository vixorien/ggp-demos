
#include "ShaderStructs.hlsli"
#include "Lighting.hlsli"



cbuffer ExternalData : register(b0)
{
	// Scene related
	Light lights[MAX_LIGHTS];
	int lightCount;

	float3 ambientColor;

	// Camera related
	float3 cameraPosition;

	// Material related
	float3 colorTint;
	float2 uvScale;
	float2 uvOffset;
}

// Texture related resources
Texture2D Albedo				: register(t0);
Texture2D NormalMap				: register(t1);
Texture2D RoughnessMap			: register(t2);
Texture2D MetalMap				: register(t3);
Texture2D ShadowMap				: register(t4);

SamplerState BasicSampler				: register(s0);
SamplerComparisonState ShadowSampler	: register(s1); // Different data type!

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	// Clean up un-normalized normals
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Adjust uv scaling
	input.uv = input.uv * uvScale + uvOffset;

	// Handle normal mapping
	input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);

	// Sample various maps for PBR
	float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;
	float metal = MetalMap.Sample(BasicSampler, input.uv).r;
	float4 surfaceColor = Albedo.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = pow(surfaceColor.rgb, 2.2f);

	// Specular color - Assuming albedo texture is actually holding specular color if metal == 1
	// Note the use of lerp here - metal is generally 0 or 1, but might be in between
	// because of linear texture sampling, so we want lerp the specular color to match
	float3 specColor = lerp(F0_NON_METAL.rrr, surfaceColor.rgb, metal);

	// SHADOW MAPPING --------------------------------
	// Note: This is only for a SINGLE light!  If you want multiple lights to cast shadows,
	// you need to do all of this multiple times IN THIS SHADER.
	float2 shadowUV = input.posForShadow.xy / input.posForShadow.w * 0.5f + 0.5f;
	shadowUV.y = 1.0f - shadowUV.y;

	// Calculate this pixel's depth from the light
	float depthFromLight = input.posForShadow.z / input.posForShadow.w;

	// Sample the shadow map using a comparison sampler, which
	// will compare the depth from the light and the value in the shadow map
	// Note: This is applied below, after we calc our DIRECTIONAL LIGHT
	float shadowAmount = ShadowMap.SampleCmpLevelZero(ShadowSampler, shadowUV, depthFromLight);

	// Start off with ambient
	float3 totalLight = ambientColor * surfaceColor.rgb;

	// Loop and handle all lights
	for (int i = 0; i < lightCount; i++)
	{
		// Grab this light and normalize the direction (just in case)
		Light light = lights[i];
		light.Direction = normalize(light.Direction);

		// Run the correct lighting calculation based on the light's type
		switch (light.Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			float3 dirLightResult = DirLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor.rgb, specColor);

			// Apply the directional light result, scaled by the shadow mapping
			//   Note: This demo really only has one shadow map, so this
			//   will only be correct for THE FIRST DIRECTIONAL LIGHT
			totalLight += dirLightResult * (light.CastsShadows ? shadowAmount : 1.0f);
			break;

		case LIGHT_TYPE_POINT:
			totalLight += PointLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;

		case LIGHT_TYPE_SPOT:
			totalLight += SpotLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor.rgb, specColor);
			break;
		}
	}

	// Should have the complete light contribution at this point.  Just need to gamma correct
	return float4(pow(totalLight, 1.0f / 2.2f), 1);
}