
#include "ShaderStructs.hlsli"
#include "Lighting.hlsli"

#define NUM_LIGHTS 5

cbuffer ExternalData : register(b0)
{
	// Scene related
	Light lights[NUM_LIGHTS];
	float3 ambientColor;

	// Camera related
	float3 cameraPosition;

	// Material related
	float3 colorTint;
	float roughness;
	float2 uvScale;
	float2 uvOffset;
	
	
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

	// Sample the texture and tint for the final surface color
	float3 surfaceColor = SurfaceTexture.Sample(BasicSampler, input.uv).rgb;
	surfaceColor *= colorTint;

	// Sample the specular map to get the scale to specular for this pixel
	float specularScale = SpecularMap.Sample(BasicSampler, input.uv).r; // Just the red channel!
	
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
	return float4(totalLight, 1);
}