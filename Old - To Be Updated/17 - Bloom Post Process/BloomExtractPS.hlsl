
cbuffer externalData : register(b0)
{
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
	float4 pixelColor = pixels.Sample(samplerOptions, input.uv);



	// Note: There are several ways you could determine if a pixel
	// is "bloom worthy" or not.  Each method gives slightly 
	// different results.



	// Method 1: Luminance ==========================================
	// Calculates the luminance ("brightness") of a pixels and compares that
	// agains the threshold.  See https://en.wikipedia.org/wiki/Relative_luminance 
	// for more info on luminance.
	//float luminance = dot(pixelColor.rgb, float3(0.2126, 0.7152, 0.0722));

	// Return this pixel's color if its luminance is over the threshold value
	//return luminance >= bloomThreshold ? pixelColor : float4(0, 0, 0, 1);



	// Method 2: Threshold Subtraction ===============================
	// From a Microsoft bloom demo that can be found here
	// https://github.com/microsoft/DirectXTK/wiki/Writing-custom-shaders
	//return saturate((pixelColor - bloomThreshold) / (1 - bloomThreshold));


	
	// Method 3: Remainder ===========================================
	// Subtract the  threshold from the color and return what's left over
	return max(pixelColor - bloomThreshold, 0);
}