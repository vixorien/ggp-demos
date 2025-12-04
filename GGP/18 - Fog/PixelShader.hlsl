
#include "ShaderStructs.hlsli"
#include "Lighting.hlsli"

cbuffer ExternalData : register(b0)
{
	Light lights[MAX_LIGHTS];
	
	int lightCount;
	float3 ambientColor;
	
	float3 cameraPosition;
	float farClipDistance;
	
	// Material and fog
	float3 colorTint;

	// Fog options
	int fogType;
	float3 fogColor;
	float fogStartDist;
	float fogEndDist;
	float fogDensity;
	int heightBasedFog;
	float fogVerticalDensity;
	float fogHeight;
	
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
	for (int i = 0; i < MAX_LIGHTS; i++)
	{
		// Grab this light and normalize the direction (just in case)
		Light light = lights[i];
		light.Direction = normalize(light.Direction);

		// Run the correct lighting calculation based on the light's type
		switch (lights[i].Type)
		{
		case LIGHT_TYPE_DIRECTIONAL: 
			totalLight += DirLight(light, input.normal, input.worldPos, cameraPosition, 0.5f, colorTint, 1.0f);
			break;

		case LIGHT_TYPE_POINT: 
			totalLight += PointLight(light, input.normal, input.worldPos, cameraPosition, 0.5f, colorTint, 1.0f);
			break;

		case LIGHT_TYPE_SPOT:
			totalLight += SpotLight(light, input.normal, input.worldPos, cameraPosition, 0.5f, colorTint, 1.0f);
			break;
		}
	}
	
	// Check the fog type
	float fog = 0.0f;
	float surfaceDistance = distance(cameraPosition, input.worldPos);

	switch (fogType)
	{
		 // Linear to far clip plane
		case 0: fog = surfaceDistance / farClipDistance; break;	
		
		// Smooth between start/end distances
		case 1: fog = smoothstep(fogStartDist, fogEndDist, surfaceDistance); break;
		
		// Exponential to far clip
		case 2: fog = 1.0f - exp(-surfaceDistance * fogDensity); break;
	}
	
	// Exponential vertical fog
	if (heightBasedFog)
	{
		float fogV = 1.0f - exp(-(fogHeight - input.worldPos.y) * fogVerticalDensity);
		fog = max(fog, fogV);
	}
	
	// Apply fog as a lerp between the final color and a fog color
	totalLight = lerp(totalLight, fogColor, saturate(fog));
	

	// Should have the complete light contribution at this point
	return float4(totalLight, 1);
}