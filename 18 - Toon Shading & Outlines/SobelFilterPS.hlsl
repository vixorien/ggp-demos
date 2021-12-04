
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
	float horizontalFilter[9] =
	{
		-1, +0, +1,
		-2, +0, +2,
		-1, +0, +1
	};

	float verticalFilter[9] =
	{
		-1, -2, -1,
		+0, +0, +0,
		+1, +2, +1
	};

	// Take 9 samples from the texture
	float3 samples[9];
	samples[0] = pixels.Sample(samplerOptions, input.uv + float2(-pixelWidth, -pixelHeight)).rgb;
	samples[1] = pixels.Sample(samplerOptions, input.uv + float2(0, -pixelHeight)).rgb;
	samples[2] = pixels.Sample(samplerOptions, input.uv + float2(pixelWidth, -pixelHeight)).rgb;
	samples[3] = pixels.Sample(samplerOptions, input.uv + float2(-pixelWidth, 0)).rgb;
	samples[4] = pixels.Sample(samplerOptions, input.uv).rgb;
	samples[5] = pixels.Sample(samplerOptions, input.uv + float2(pixelWidth, 0)).rgb;
	samples[6] = pixels.Sample(samplerOptions, input.uv + float2(-pixelWidth, pixelHeight)).rgb;
	samples[7] = pixels.Sample(samplerOptions, input.uv + float2(0, pixelHeight)).rgb;
	samples[8]  = pixels.Sample(samplerOptions, input.uv + float2(pixelWidth, pixelHeight)).rgb;

	float3 horizontal = float3(0, 0, 0);
	float3 vertical = float3(0, 0, 0);

	for (int i = 0; i < 9; i++)
	{
		horizontal += samples[i] * horizontalFilter[i];
		vertical += samples[i] * verticalFilter[i];
	}

	float3 sobel = sqrt(horizontal * horizontal + vertical * vertical);
	float sobelTotal = saturate(sobel.x + sobel.y + sobel.z);

	float3 finalColor = lerp(samples[4], float3(0, 0, 0), sobelTotal);

	return float4(finalColor, 1);
}