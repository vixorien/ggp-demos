
#include "ShaderStructs.hlsli"
#include "Lighting.hlsli"

#define NUM_LIGHTS 5

cbuffer ExternalData : register(b0)
{
	float roughness;
	float3 colorTint;
	float3 ambientColor;
	float3 cameraPosition;

	Light lights[NUM_LIGHTS];
}


// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	// Clean up un-normalized normals
	input.normal = normalize(input.normal);

	// Start off with ambient
	float3 totalLight = ambientColor * colorTint;
	
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
			totalLight += DirLight(light, input.normal, input.worldPos, cameraPosition, roughness, colorTint);
			break;

		case LIGHT_TYPE_POINT: 
			totalLight += PointLight(light, input.normal, input.worldPos, cameraPosition, roughness, colorTint);
			break;

		case LIGHT_TYPE_SPOT:
			totalLight += SpotLight(light, input.normal, input.worldPos, cameraPosition, roughness, colorTint);
			break;
		}
	}

	// Should have the complete light contribution at this point
	return float4(totalLight, 1);
}