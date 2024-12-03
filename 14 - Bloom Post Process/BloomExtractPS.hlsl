
cbuffer externalData : register(b0)
{
	int extractType;
	float bloomThreshold;
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

float4 main(VertexToPixel input) : SV_TARGET
{
	// Grab this pixel's color
	float3 pixelColor = pixels.Sample(samplerOptions, input.uv).rgb;

	switch (extractType)
	{
		case 0:
			// Remainder ===========================================
			// Subtract the threshold from the color and return what's left over
			return float4(max(pixelColor - bloomThreshold, 0), 1.0f);
		
		case 1:
			// Threshold Ratio ===============================
			// From a Microsoft bloom demo that can be found here
			// https://github.com/microsoft/DirectXTK/wiki/Writing-custom-shaders
			return float4(saturate((pixelColor - bloomThreshold) / (1 - bloomThreshold)), 1);
		
		case 2:
			// Luminance ===========================================
			// Calculates the luminance ("brightness") of a pixels and compares that
			// agains the threshold.  See https://en.wikipedia.org/wiki/Relative_luminance 
			// for more info on luminance.
			float luminance = dot(pixelColor, float3(0.2126, 0.7152, 0.0722));

			// Return this pixel's color if its luminance is over the threshold value
			return float4(luminance >= bloomThreshold ? pixelColor : float3(0, 0, 0), 1);
	}
	
	// Incorrect extract type, just return black
	return float4(0,0,0,1);
}