
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
	int gammaCorrection;
	int useMetalMap;
	int useNormalMap;
	int useRoughnessMap;
	int useAlbedoTexture;
	int useBurleyDiffuse;
}

// Texture related resources
Texture2D Albedo				: register(t0);
Texture2D NormalMap				: register(t1);
Texture2D RoughnessMap			: register(t2);
Texture2D MetalMap				: register(t3);
SamplerState BasicSampler		: register(s0);

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

	// Use normal mapping
	float3 normalMap = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);
	input.normal = useNormalMap ? normalMap : input.normal;

	// Sample the roughness map - this essentially becomes our "specular map" in non-PBR
	float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;
	roughness = useRoughnessMap ? roughness : 0.2f;

	// Sample the metal map
	float metal = MetalMap.Sample(BasicSampler, input.uv).r;
	metal = useMetalMap ? metal : 0.0f;

	// Sample texture
	float4 surfaceColor = Albedo.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = gammaCorrection ? pow(surfaceColor.rgb, 2.2) : surfaceColor.rgb;

	// Actually using texture?
	surfaceColor.rgb = useAlbedoTexture ? surfaceColor.rgb : colorTint.rgb;

	// Specular color - Assuming albedo texture is actually holding specular color if metal == 1
	// Note the use of lerp here - metal is generally 0 or 1, but might be in between
	// because of linear texture sampling, so we want lerp the specular color to match
	float3 specColor = lerp(F0_NON_METAL, surfaceColor.rgb, metal);

	// Start off with ambient
	float3 totalLight = ambientColor * surfaceColor.rgb;

	// Loop and handle all lights
	for (int i = 0; i < lightCount; i++)
	{
		// Grab this light and normalize the direction (just in case)
		Light light = lights[i];
		light.Direction = normalize(light.Direction);

		// Run the correct lighting calculation based on the light's type
		switch (lights[i].Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			totalLight += DirLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor.rgb, specColor, useBurleyDiffuse);
			break;

		case LIGHT_TYPE_POINT:
			totalLight += PointLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor.rgb, specColor, useBurleyDiffuse);
			break;

		case LIGHT_TYPE_SPOT:
			totalLight += SpotLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor.rgb, specColor, useBurleyDiffuse);
			break;
		}
	}

	// Should have the complete light contribution at this point. 
	// Gamma correct if necessary
	float3 final = gammaCorrection ? pow(totalLight, 1.0f / 2.2f) : totalLight;
	return float4(final, 1);
}