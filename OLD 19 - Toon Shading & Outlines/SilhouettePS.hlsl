
cbuffer ExternalData : register(b0)
{
	float pixelWidth;
	float pixelHeight;
}


// Defines the input to this pixel shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

// Textures and such
Texture2D pixels			: register(t0);
SamplerState samplerOptions	: register(s0);


// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
	
	// Take 9 samples from the texture
	float4 samples[9];
	samples[0] = pixels.Sample(samplerOptions, input.uv + float2(-pixelWidth, -pixelHeight));
	samples[1] = pixels.Sample(samplerOptions, input.uv + float2(0, -pixelHeight));
	samples[2] = pixels.Sample(samplerOptions, input.uv + float2(pixelWidth, -pixelHeight));
	samples[3] = pixels.Sample(samplerOptions, input.uv + float2(-pixelWidth, 0));
	samples[4] = pixels.Sample(samplerOptions, input.uv); // THIS PIXEL!
	samples[5] = pixels.Sample(samplerOptions, input.uv + float2(pixelWidth, 0));
	samples[6] = pixels.Sample(samplerOptions, input.uv + float2(-pixelWidth, pixelHeight));
	samples[7] = pixels.Sample(samplerOptions, input.uv + float2(0, pixelHeight));
	samples[8] = pixels.Sample(samplerOptions, input.uv + float2(pixelWidth, pixelHeight));

	// Compare the IDs of surrounding pixels to the ID of this pixel (samples[4])
	bool same =
		samples[0].a == samples[4].a &&
		samples[1].a == samples[4].a &&
		samples[2].a == samples[4].a &&
		samples[3].a == samples[4].a &&
		samples[5].a == samples[4].a &&
		samples[6].a == samples[4].a &&
		samples[7].a == samples[4].a &&
		samples[8].a == samples[4].a;
	
	// Which color do we use?
	float3 finalColor = same ? samples[4].rgb : float3(0, 0, 0);
	return float4(finalColor, 1);
}