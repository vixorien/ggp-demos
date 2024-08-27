
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
Texture2D BlendMap				: register(t0);

Texture2D Albedo0				: register(t1);
Texture2D NormalMap0			: register(t2);
Texture2D RoughnessMap0			: register(t3);
Texture2D MetalMap0				: register(t4);

Texture2D Albedo1				: register(t5);
Texture2D NormalMap1			: register(t6);
Texture2D RoughnessMap1			: register(t7);
Texture2D MetalMap1				: register(t8);

Texture2D Albedo2				: register(t9);
Texture2D NormalMap2			: register(t10);
Texture2D RoughnessMap2			: register(t11);
Texture2D MetalMap2				: register(t12);

SamplerState BasicSampler				: register(s0);


float3 Blend(float4 blendAmounts, float3 value0, float3 value1, float3 value2)
{
	return 
		value0 * blendAmounts.r + 
		value1 * blendAmounts.g + 
		value2 * blendAmounts.b;
}

float Blend(float4 blendAmounts, float value0, float value1, float value2)
{
	return
		value0 * blendAmounts.r +
		value1 * blendAmounts.g +
		value2 * blendAmounts.b;
}

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	// Clean up un-normalized normals
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Sample splat map at standard UV scaling
	float4 blendValues = BlendMap.Sample(BasicSampler, input.uv);

	// Adjust uv scaling for other maps
	input.uv = input.uv * uvScale + uvOffset;

	// Blend normals
	float3 normalFromMap = Blend(blendValues,
		SampleAndUnpackNormalMap(NormalMap0, BasicSampler, input.uv),
		SampleAndUnpackNormalMap(NormalMap1, BasicSampler, input.uv),
		SampleAndUnpackNormalMap(NormalMap2, BasicSampler, input.uv));

	// Apply normal mapping
	input.normal = NormalMapping(normalFromMap, input.normal, input.tangent);
	
	// Blend other values
	float roughness = Blend(blendValues,
		RoughnessMap0.Sample(BasicSampler, input.uv).r,
		RoughnessMap1.Sample(BasicSampler, input.uv).r,
		RoughnessMap2.Sample(BasicSampler, input.uv).r);
	
	float metal = Blend(blendValues,
		MetalMap0.Sample(BasicSampler, input.uv).r,
		MetalMap1.Sample(BasicSampler, input.uv).r,
		MetalMap2.Sample(BasicSampler, input.uv).r);

	float3 surfaceColor = Blend(blendValues,
		pow(Albedo0.Sample(BasicSampler, input.uv).rgb, 2.2f),
		pow(Albedo1.Sample(BasicSampler, input.uv).rgb, 2.2f),
		pow(Albedo2.Sample(BasicSampler, input.uv).rgb, 2.2f));

	// Specular color - Assuming albedo texture is actually holding specular color if metal == 1
	// Note the use of lerp here - metal is generally 0 or 1, but might be in between
	// because of linear texture sampling, so we want lerp the specular color to match
	float3 specColor = lerp(F0_NON_METAL.rrr, surfaceColor, metal);

	// Start off with ambient
	float3 totalLight = ambientColor * surfaceColor;

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
			totalLight += DirLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor, specColor);
			break;

		case LIGHT_TYPE_POINT:
			totalLight += PointLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor, specColor);
			break;

		case LIGHT_TYPE_SPOT:
			totalLight += SpotLightPBR(light, input.normal, input.worldPos, cameraPosition, roughness, metal, surfaceColor, specColor);
			break;
		}
	}

	// Should have the complete light contribution at this point.  Just need to gamma correct
	return float4(pow(totalLight, 1.0f / 2.2f), 1);
}