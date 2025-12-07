
#include "ShaderStructs.hlsli"
#include "Lighting.hlsli"
#include "ToonShading.hlsli"


cbuffer ExternalData : register(b0)
{
	// Scene related
	Light lights[MAX_LIGHTS];
	
	int lightCount;
	float3 ambientColor;

	// Camera related
	float3 cameraPosition;
	float pad;

	// Material related
	float3 colorTint;
	float pad2;
	
	float2 uvScale;
	float2 uvOffset;
	
	int toonShadingType; // The type of toon shading to apply
	int silhouetteID; // Non-negative ID value for use with silhouette post process
}

// Texture related resources
Texture2D Albedo				: register(t0);
Texture2D NormalMap				: register(t1);
Texture2D RoughnessMap			: register(t2);
Texture2D ToonRamp				: register(t3);
Texture2D ToonRampSpecular		: register(t4);
SamplerState BasicSampler		: register(s0);
SamplerState ClampSampler		: register(s1);


// Struct that allows our pixel shader to return multiple
// pieces of data (usually colors), each of which will go
// to a different active render target.  
// Note: This is only being used by the Depth & Normals Outline post process
//       but I'm including it here instead of making a separate shader that is 99% identical
struct PSOutput
{
	float4 color	: SV_TARGET0;
	float4 normals	: SV_TARGET1;
	float depth		: SV_TARGET2;
};


// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
PSOutput main(VertexToPixel input)
{
	// Clean up un-normalized normals
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);

	// Adjust uv scaling
	input.uv = input.uv * uvScale + uvOffset;

	// Use normal mapping
	input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);

	// Sample the roughness map - this essentially becomes our "specular map" in non-PBR
	float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;

	// Sample texture and gamma un-correct
	float4 surfaceColor = pow(Albedo.Sample(BasicSampler, input.uv), 2.2f);
	surfaceColor.rgb *= colorTint;

	// Start off with ambient
	float3 totalLight = ambientColor * surfaceColor.rgb;

	// Loop and handle all lights
	// Note: Toon shading probably just wants a single light,
	//       but I'm leaving the loop here for experimentation purposes
	for (int i = 0; i < lightCount; i++)
	{
		// Grab this light and normalize the direction (just in case)
		Light light = lights[i];
		light.Direction = normalize(light.Direction);

		// Grab other necessary vectors
		float3 toLight = float3(0, 0, 0); // Replaced below
		float3 toCam = normalize(cameraPosition - input.worldPos);
		float atten = 1.0f;		// Point/spot lights
		float penumbra = 1.0f;	// Spot lights

		// Popped the vector and adjustment calculations out of the
		// helper methods below, so we can alter diffuse/spec with
		// the toon ramp before combining
		switch (light.Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			// Just the to-light vector, which is negative direction
			toLight = normalize(-light.Direction);
			break;

		case LIGHT_TYPE_POINT:
			// Calc proper to-light and attenuation
			toLight = normalize(light.Position - input.worldPos);
			atten = Attenuate(light, input.worldPos);
			break;

		case LIGHT_TYPE_SPOT:
			// Calc proper to-light, attenuation and spot falloff
			toLight = normalize(light.Position - input.worldPos);
			atten = Attenuate(light, input.worldPos);
			penumbra = pow(saturate(dot(-toLight, normalize(light.Direction))), light.SpotFalloff);
			break;
		}

		// Run non-PBR diffuse and specular
		float diffuse = Diffuse(input.normal, toLight);
		float spec = SpecularPhong(input.normal, toLight, toCam, roughness);

		// Any toon shading to apply to the lighting?
		// NOTE: Generally you would be applying exactly one of these two options, rather
		//       than having a switch or other conditional here - I'm just putting it all
		//       in a single shader so I can swap between the modes easily.
		switch (toonShadingType)
		{
		case TOON_SHADING_RAMP:
			// Apply a ramp to each lighting result
			diffuse = ApplyToonShadingRamp(diffuse, ToonRamp, ClampSampler);
			spec = ApplyToonShadingRamp(spec, ToonRampSpecular, ClampSampler);
			break;

		case TOON_SHADING_CONDITIONALS:
			// Use conditionals to alter the lighting
			diffuse = ApplyToonShadingDiffuseConditionals(diffuse);
			spec = ApplyToonShadingSpecularConditionals(spec);
			break;
		}

		// Final light color
		totalLight += (diffuse * surfaceColor.rgb + spec) * atten * penumbra * light.Intensity * light.Color;
	}

	// Final color with gamma correction
	// NOTE: Storing the silhouette ID in the alpha channel, since we're not actually using that for anything
	//       - This works now since we're not doing any transparency (blending is OFF by default)
	//       - If we WERE using transparency, we'd have to store this another way (2nd render target, perhaps)
	float4 finalColor = float4(pow(totalLight, 1.0f / 2.2f), silhouetteID / 256.0f);

	// Set up the output struct, which allows a single pixel shader to output multiple colors
	// with each color going to a different, active render target.
	PSOutput output;
	output.color = finalColor;
	output.normals = float4(input.normal, 1);
	output.depth = input.screenPosition.z;
	return output;
}