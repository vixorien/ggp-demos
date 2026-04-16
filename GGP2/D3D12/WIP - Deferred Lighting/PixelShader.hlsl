#include "Lighting.hlsli"

cbuffer BindlessData : register(b0)
{
	uint vsVertexBufferIndex;
	uint vsPerFrameCBIndex;
	uint vsPerObjectCBIndex;
	uint psPerFrameCBIndex;
	uint psPerObjectCBIndex;
}

// Alignment matters!!!
struct PSPerFrameData
{
	float3 cameraPosition;
	int lightCount;
	Light lights[MAX_LIGHTS];
};

struct PSPerObjectData
{
	uint albedoIndex;
	uint normalMapIndex;
	uint roughnessIndex;
	uint metalnessIndex;
	float2 uvScale;
	float2 uvOffset;
};

// Struct representing the data we expect to receive from earlier pipeline stages
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION;
};

struct PS_Output
{
	float4 Color      : SV_TARGET0;
	float4 Normal     : SV_TARGET1;
	float4 Material   : SV_TARGET2;
	float  Depth       : SV_TARGET3;
};

// Texture related
SamplerState BasicSampler		: register(s0);

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
PS_Output main(VertexToPixel input)
{
	ConstantBuffer<PSPerFrameData> cbFrame = ResourceDescriptorHeap[psPerFrameCBIndex];
	ConstantBuffer<PSPerObjectData> cbObject = ResourceDescriptorHeap[psPerObjectCBIndex];
	
	Texture2D AlbedoTexture = ResourceDescriptorHeap[cbObject.albedoIndex];
	Texture2D NormalMap		= ResourceDescriptorHeap[cbObject.normalMapIndex];
	Texture2D RoughnessMap	= ResourceDescriptorHeap[cbObject.roughnessIndex];
	Texture2D MetalMap		= ResourceDescriptorHeap[cbObject.metalnessIndex];
	
	// Clean up un-normalized normals
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);
	
	// Scale and offset uv as necessary
	input.uv = input.uv * cbObject.uvScale + cbObject.uvOffset;

	// Normal mapping
	input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);

	// Surface color with gamma correction
	float4 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
	surfaceColor.rgb = pow(surfaceColor.rgb, 2.2);
	
	// Sample the other maps
	float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;
	float metal = MetalMap.Sample(BasicSampler, input.uv).r;
	
	// Specular color - Assuming albedo texture is actually holding specular color if metal == 1
	// Note the use of lerp here - metal is generally 0 or 1, but might be in between
	// because of linear texture sampling, so we want lerp the specular color to match
	float3 specColor = lerp(F0_NON_METAL.rrr, surfaceColor.rgb, metal);

	// Keep a running total of light
	float3 totalLight = float3(0,0,0);

	// Loop and handle all lights
	for (int i = 0; i < cbFrame.lightCount; i++)
	{
		// Grab this light and normalize the direction (just in case)
		Light light = cbFrame.lights[i];
		light.Direction = normalize(light.Direction);

		// Run the correct lighting calculation based on the light's type
		switch (light.Type)
		{
		case LIGHT_TYPE_DIRECTIONAL:
			totalLight += DirLightPBR(light, input.normal, input.worldPos, cbFrame.cameraPosition, roughness, metal, surfaceColor.rgb, specColor, 0);
			break;

		case LIGHT_TYPE_POINT:
			totalLight += PointLightPBR(light, input.normal, input.worldPos, cbFrame.cameraPosition, roughness, metal, surfaceColor.rgb, specColor, 0);
			break;

		case LIGHT_TYPE_SPOT:
			totalLight += SpotLightPBR(light, input.normal, input.worldPos, cbFrame.cameraPosition, roughness, metal, surfaceColor.rgb, specColor, 0);
			break;
		}
	}

	// Set up return struct
	PS_Output output;
	output.Color = float4(pow(totalLight, 1.0f / 2.2f), 1.0f);
	output.Normal = float4(input.normal * 0.5f + 0.5f, 1.0f);
	output.Material = float4(roughness, metal, 0, 1);
	output.Depth = input.screenPosition.z;
	return output;
}