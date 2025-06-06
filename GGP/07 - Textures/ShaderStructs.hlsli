#ifndef __GGP_SHADER_STRUCTS__
#define __GPP_SHADER_STRUCTS__

// Structs for various shaders

// Basic VS input for a standard Pos/UV/Normal vertex
struct VertexShaderInput
{
	float3 localPosition	: POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
};



// VS Output / PS Input struct for basic lighting
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
};





#endif