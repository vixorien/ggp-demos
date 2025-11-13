
#include "ShaderStructs.hlsli"
#include "Lighting.hlsli"

#define NUM_LIGHTS 5

cbuffer ExternalData : register(b0)
{
	// Scene related
	Light lights[MAX_LIGHTS];
	float3 ambientColor;

	// Camera related
	float3 cameraPosition;

	// Material related
	float3 colorTint;
	float roughness;
	float2 uvScale;
	float2 uvOffset;
	int useSpecularMap;
	int gammaCorrection;
}

// Texture related resources
Texture2D SurfaceTexture	: register(t0); // Textures use "t" registers
Texture2D SpecularMap		: register(t1);

SamplerState BasicSampler	: register(s0); // Samplers use "s" registers

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	// Clean up un-normalized normals
	input.normal = normalize(input.normal);

	// Adjust the uv coords
	input.uv = input.uv * uvScale + uvOffset;

	// Sample the texture and "un-gamma-correct" if necessary
	float3 surfaceColor = SurfaceTexture.Sample(BasicSampler, input.uv).rgb;
	if (gammaCorrection)
		surfaceColor = pow(surfaceColor, 2.2f);

	// Tint by material color
	surfaceColor *= colorTint;

	// Sample the specular map to get the scale to specular for this pixel
	float specularScale = 1.0f;
	if (useSpecularMap)
	{
		// Are we actually using the spec map?
		specularScale = SpecularMap.Sample(BasicSampler, input.uv).r; // Just the red channel!
	}
	
	// Start off with ambient
	float3 totalLight = ambientColor * surfaceColor;
	
	// Loop and handle all lights
	for (int i = 0; i < NUM_LIGHTS; i++)
	{
		// Grab this light and normalize the direction (just in case)
		Light light = lights[i];
		light.Direction = normalize(light.Direction);

		// Run the correct lighting calculation based on the light's type
		switch (lights[i].Type)
		{
		case LIGHT_TYPE_DIRECTIONAL: 
			totalLight += DirLight(light, input.normal, input.worldPos, cameraPosition, roughness, surfaceColor, specularScale);
			break;

		case LIGHT_TYPE_POINT: 
			totalLight += PointLight(light, input.normal, input.worldPos, cameraPosition, roughness, surfaceColor, specularScale);
			break;

		case LIGHT_TYPE_SPOT:
			totalLight += SpotLight(light, input.normal, input.worldPos, cameraPosition, roughness, surfaceColor, specularScale);
			break;
		}
	}

	// Should have the complete light contribution at this point
	// Gamma correct if necessary
	return float4(gammaCorrection ? pow(totalLight, 1.0f / 2.2f) : totalLight, 1);
}