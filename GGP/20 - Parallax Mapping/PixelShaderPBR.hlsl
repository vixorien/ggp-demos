
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
	
	// Parallax
	float heightScale;
	int parallaxSamples;
}

// Texture related resources
Texture2D Albedo				: register(t0);
Texture2D NormalMap				: register(t1);
Texture2D RoughnessMap			: register(t2);
Texture2D MetalMap				: register(t3);
Texture2D HeightMap				: register(t4);
SamplerState BasicSampler		: register(s0);


float2 GetParallaxUV(float2 uv, float3 view, float3x3 TBN, int samples, float scale)
{
	// Get tangent space view vector
	// Note: Multiplying in opposite order is effectively
	//       transposing the matrix, which acts like an 
	//       invert on a pure 3x3 rotation matrix!
	float3 view_TS = mul(TBN, view); // World to Tangent space

	// Calculate the ray direction, including proper length based on scale
	float viewLength = length(view_TS);
	float parallaxLength = sqrt(viewLength * viewLength - view_TS.z * view_TS.z) / view_TS.z;
	float2 rayDir = normalize(view_TS.xy) * parallaxLength * scale;
	
	// Tracking height and position during raymarch
	float currentHeight = 1.0f;
	float2 currentPos = uv;
	float stepSize = 1.0f / samples;
	float2 uvStep = rayDir * stepSize;
	
	// Calculate uv derivates to support texture sampling
	// in a loop with variable iterations
	float2 dx = ddx(uv);
	float2 dy = ddy(uv);

	// Raymarch through surface
	for (int i = 0; i < samples; i++)
	{
		// Offset along ray and grab the height there
		currentPos -= uvStep;
		currentHeight -= stepSize;
		float heightAtPos = HeightMap.SampleGrad(BasicSampler, currentPos, dx, dy).r;

		// If we've gone "below" the heightmap, we've hit!
		if (currentHeight < heightAtPos)
		{
			break;
		}
	}

	return currentPos;
}


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

	// Handle normal mapping - Placing this code directly in
	// the shader as we'll need TBN for parallax mapping, too
	
	// Gather the required vectors for converting the normal
	float3 N = input.normal;
	float3 T = normalize(input.tangent - N * dot(input.tangent, N));
	float3 B = cross(T, N);

	// Create the 3x3 matrix to convert from TANGENT-SPACE normals to WORLD-SPACE normals
	float3x3 TBN = float3x3(T, B, N);
	
	// Handle parallax mapping
	float3 v = normalize(cameraPosition - input.worldPos);
	input.uv = GetParallaxUV(input.uv, v, float3x3(T, -B, N), parallaxSamples, heightScale); // Invert Y in TBN matrix!

	// Finalize normal mapping after parallax
	float3 normalFromMap = SampleAndUnpackNormalMap(NormalMap, BasicSampler, input.uv);
	input.normal = normalize(mul(normalFromMap, TBN));

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