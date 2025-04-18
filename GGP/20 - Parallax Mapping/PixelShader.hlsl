
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
}

// Texture related resources
Texture2D Albedo : register(t0);
Texture2D NormalMap : register(t1);
Texture2D RoughnessMap : register(t2);
// Note: Our "old school" lighting model doesn't take metalness into account, so no metal map!
SamplerState BasicSampler : register(s0);

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

	// Sample texture
	float4 surfaceColor = Albedo.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = gammaCorrection ? pow(surfaceColor.rgb, 2.2) : surfaceColor.rgb;

	// Actually using texture?
	surfaceColor.rgb = useAlbedoTexture ? surfaceColor.rgb : colorTint.rgb;
	
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
				totalLight += DirLight(light, input.normal, input.worldPos, cameraPosition, roughness, surfaceColor.rgb, 1.0f - roughness); // Using roughness as spec map in non-PBR
				break;

			case LIGHT_TYPE_POINT:
				totalLight += PointLight(light, input.normal, input.worldPos, cameraPosition, roughness, surfaceColor.rgb, 1.0f - roughness); // Using roughness as spec map in non-PBR
				break;

			case LIGHT_TYPE_SPOT:
				totalLight += SpotLight(light, input.normal, input.worldPos, cameraPosition, roughness, surfaceColor.rgb, 1.0f - roughness); // Using roughness as spec map in non-PBR
				break;
		}
	}

	// Should have the complete light contribution at this point. 
	// Gamma correct if necessary
	float3 final = gammaCorrection ? pow(totalLight, 1.0f / 2.2f) : totalLight;
	return float4(final, 1);
}