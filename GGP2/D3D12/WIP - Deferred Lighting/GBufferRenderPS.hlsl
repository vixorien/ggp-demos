#include "Lighting.hlsli"

cbuffer BindlessData : register(b0)
{
	uint vsVertexBufferIndex;
	uint vsPerFrameCBIndex;
	uint vsPerObjectCBIndex;
	uint psPerFrameCBIndex;
	uint psPerObjectCBIndex;
}

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
	float4 Albedo      : SV_TARGET0;
	float4 Normal      : SV_TARGET1;
	float4 Material    : SV_TARGET2;
	float  Depth       : SV_TARGET3;
};

// Texture related
SamplerState BasicSampler		: register(s0);

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
PS_Output main(VertexToPixel input)
{
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
	float3 surfaceColor = AlbedoTexture.Sample(BasicSampler, input.uv).rgb;
	surfaceColor = pow(surfaceColor, 2.2);
	
	// Sample the other maps
	float roughness = RoughnessMap.Sample(BasicSampler, input.uv).r;
	float metal = MetalMap.Sample(BasicSampler, input.uv).r;

	// Gbuffer output with NO gamma correction
	PS_Output output;
	output.Albedo     = float4(surfaceColor, 1.0f);
	output.Normal     = float4(input.normal * 0.5f + 0.5f, 1.0f);
	output.Material   = float4(roughness, metal, 0, 1);
	output.Depth      = input.screenPosition.z;
	return output;
}