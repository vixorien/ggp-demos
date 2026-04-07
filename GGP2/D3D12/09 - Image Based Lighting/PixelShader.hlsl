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
	
	uint irradianceIndex;
	uint specularIndex;
	uint brdfLUTIndex;
	uint totalSpecularMipLevels;
	
	Light lights[MAX_LIGHTS];
	
	float4 SHColors[9];
	uint UseSH;
	
	uint IndirectLightingEnabled;
};

struct PSPerObjectData
{
	uint albedoIndex;
	uint normalMapIndex;
	uint roughnessIndex;
	uint metalnessIndex;
	float2 uvScale;
	float2 uvOffset;
	float3 colorTint;
	float roughness;
	float metalness;
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

// Samplers
SamplerState BasicSampler : register(s0);
SamplerState ClampSampler : register(s1);

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	ConstantBuffer<PSPerFrameData> cbFrame = ResourceDescriptorHeap[psPerFrameCBIndex];
	ConstantBuffer<PSPerObjectData> cbObject = ResourceDescriptorHeap[psPerObjectCBIndex];
	
	// Clean up un-normalized normals
	input.normal = normalize(input.normal);
	input.tangent = normalize(input.tangent);
	
	// Scale and offset uv as necessary
	input.uv = input.uv * cbObject.uvScale + cbObject.uvOffset;
	
	// Default material details
	float4 surfaceColor = float4(cbObject.colorTint, 1);
	float roughness = cbObject.roughness;
	float metal = cbObject.metalness;
	
	// Are we using textures?
	if(cbObject.albedoIndex != -1)
	{
		Texture2D AlbedoTexture = ResourceDescriptorHeap[cbObject.albedoIndex];
		Texture2D NormalMap = ResourceDescriptorHeap[cbObject.normalMapIndex];
		Texture2D RoughnessMap = ResourceDescriptorHeap[cbObject.roughnessIndex];
		Texture2D MetalMap = ResourceDescriptorHeap[cbObject.metalnessIndex];
	
		// Normal mapping
		input.normal = NormalMapping(NormalMap, BasicSampler, input.uv, input.normal, input.tangent);
		
		// Surface color with gamma correction
		surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv);
		surfaceColor.rgb = pow(surfaceColor.rgb, 2.2);
	
		// Sample the other maps
		roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;
		metal = MetalMap.Sample(BasicSampler, input.uv).r;
	}
	
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
	
	// --- Indirect lighting ---
	
	// Textures
	TextureCube IrradianceMap = ResourceDescriptorHeap[cbFrame.irradianceIndex];
	TextureCube SpecularMap = ResourceDescriptorHeap[cbFrame.specularIndex];
	Texture2D BrdfLUT = ResourceDescriptorHeap[cbFrame.brdfLUTIndex];
	
	// -- Diffuse IBL --
	float3 indirectDiffuse = 0;
	if(cbFrame.UseSH)
		indirectDiffuse = GetSHColor(input.normal, cbFrame.SHColors);	
	else
		indirectDiffuse = IrradianceMap.Sample(BasicSampler, input.normal).rgb;
	
	// -- Specular look up table --
	float3 viewToCam = normalize(cbFrame.cameraPosition - input.worldPos);
	float NdotV = saturate(dot(input.normal, viewToCam));
	float2 uv = float2(NdotV, roughness);
	float2 indirectBRDF = BrdfLUT.Sample(ClampSampler, uv).rg;
	
	// -- Specular IBL --
	float3 indSpecFresnel = specColor * indirectBRDF.x + indirectBRDF.y;
	float3 viewRefl = reflect(-viewToCam, input.normal);
	float mip = roughness * (cbFrame.totalSpecularMipLevels - 1.0);
	float3 indirectSpecular = SpecularMap.SampleLevel(BasicSampler, viewRefl, mip).rgb * indSpecFresnel;
	
	// -- Total Indirect --
	float3 fullIndirect = 
		(indirectDiffuse * surfaceColor.rgb * saturate(1.0f - metal)) + 
		indirectSpecular;
	
	// Add indirect if necessary
	totalLight += cbFrame.IndirectLightingEnabled ? fullIndirect : 0;

	// Gamma correct and return
	return float4(pow(totalLight, 1.0f / 2.2f), 1.0f);
}