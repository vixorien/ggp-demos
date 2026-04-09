
#include "Lighting.hlsli"

cbuffer BindlessData : register(b0)
{
	uint vsVertexBufferIndex;
	uint vsPerFrameCBIndex;
	uint vsPerObjectCBIndex;
	uint psPerFrameCBIndex;
	uint psPerObjectCBIndex;
}

struct PSData
{
	float4 SHColors[9];
	uint UseSH;
	uint SkyboxIndex;
};

// Texture-related resources
SamplerState BasicSampler	: register(s0);

struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float3 worldPos			: POSITION;
};

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	ConstantBuffer<PSData> cb = ResourceDescriptorHeap[psPerObjectCBIndex];
	
	input.normal = normalize(input.normal);
	
	float3 color = 0;
	if (cb.UseSH)
	{
		color = GetSHColor(input.normal, cb.SHColors);
	}
	else
	{
		TextureCube IrrTexture = ResourceDescriptorHeap[cb.SkyboxIndex];
		color = IrrTexture.Sample(BasicSampler, input.normal);
	}
		
	return float4(pow(color, 1.0f / 2.2f), 1);
	
}