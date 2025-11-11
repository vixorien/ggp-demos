
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

}

// Texture related resources
Texture2D SurfaceTexture	: register(t0); // Textures use "t" registers
Texture2D SpecularMap		: register(t1);
Texture2D NormalMap			: register(t2);
TextureCube EnvironmentMap	: register(t3);

SamplerState BasicSampler	: register(s0); // Samplers use "s" registers

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	// Clean up un-normalized normals and tangents
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Adjust the uv coords
	input.uv = input.uv * uvScale + uvOffset;

	// Apply normal mapping
	input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);

	// Sample the texture and tint for the final surface color
	float3 surfaceColor = SurfaceTexture.Sample(BasicSampler, input.uv).rgb;
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

	// Sample the environment map using the reflected view vector
	float3 viewVector = normalize(cameraPosition - input.worldPos);
	float3 reflectionVector = reflect(-viewVector, input.normal); // Need camera to pixel vector, so negate
	float3 reflectionColor = EnvironmentMap.Sample(BasicSampler, reflectionVector).rgb;
	
	// Interpolate between the surface color and reflection color using a Fresnel term
	float3 finalColor = lerp(totalLight, reflectionColor, SimpleFresnel(input.normal, viewVector, F0_NON_METAL));

	// Should have the complete light contribution at this point
	return float4(finalColor, 1);
}