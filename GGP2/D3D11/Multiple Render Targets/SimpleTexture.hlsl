
// Defines the input to this pixel shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

// Textures and such
Texture2D PixelColors		: register(t0);

// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
	return PixelColors.Load(float3(input.position.xy, 0));
}