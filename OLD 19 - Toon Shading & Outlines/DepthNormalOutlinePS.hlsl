
cbuffer ExternalData : register(b0)
{
	float pixelWidth;
	float pixelHeight;
	float depthAdjust;
	float normalAdjust;
}


// Defines the input to this pixel shader
struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
};

// Textures and such
Texture2D pixels			: register(t0);
Texture2D normals			: register(t1);
Texture2D depth				: register(t2);
SamplerState samplerOptions	: register(s0);


// Entry point for this pixel shader
float4 main(VertexToPixel input) : SV_TARGET
{
	// Only doing 4 surrounding pixels instead of all 8
	float2 offsets[4] =
	{
		float2(-pixelWidth, 0),
		float2(+pixelWidth, 0),
		float2(0, -pixelHeight),
		float2(0, +pixelHeight),
	};

	// COMPARE DEPTHS --------------------------

	// Sample the depths of this pixel and the surrounding pixels
	float depthHere		= depth.Sample(samplerOptions, input.uv).r;
	float depthLeft		= depth.Sample(samplerOptions, input.uv + offsets[0]).r;
	float depthRight	= depth.Sample(samplerOptions, input.uv + offsets[1]).r;
	float depthUp		= depth.Sample(samplerOptions, input.uv + offsets[2]).r;
	float depthDown		= depth.Sample(samplerOptions, input.uv + offsets[3]).r;
	
	// Calculate how the depth changes by summing the absolute values of the differences
	float depthChange =
		abs(depthHere - depthLeft) +
		abs(depthHere - depthRight) +
		abs(depthHere - depthUp) +
		abs(depthHere - depthDown);

	float depthTotal = pow(saturate(depthChange), depthAdjust);

	// COMPARE NORMALS --------------------------

	// Sample the normals of this pixel and the surrounding pixels
	float3 normalHere	= normals.Sample(samplerOptions, input.uv).rgb;
	float3 normalLeft	= normals.Sample(samplerOptions, input.uv + offsets[0]).rgb;
	float3 normalRight	= normals.Sample(samplerOptions, input.uv + offsets[1]).rgb;
	float3 normalUp		= normals.Sample(samplerOptions, input.uv + offsets[2]).rgb;
	float3 normalDown	= normals.Sample(samplerOptions, input.uv + offsets[3]).rgb;

	// Calculate how the normal changes by summing the absolute values of the differences
	float3 normalChange =
		abs(normalHere - normalLeft) +
		abs(normalHere - normalRight) +
		abs(normalHere - normalUp) +
		abs(normalHere - normalDown);

	// Total the components
	float normalTotal = pow(saturate(normalChange.x + normalChange.y + normalChange.z), normalAdjust);

	// FINAL COLOR VALUE -----------------------------
	
	// Which result, depth or normal, is more impactful?
	float outline = max(depthTotal, normalTotal);

	// Sample the color here
	float3 color = pixels.Sample(samplerOptions, input.uv).rgb;

	// Interpolate between this color and the outline
	float3 finalColor = lerp(color, float3(0, 0, 0), outline);
	return float4(finalColor, 1);
}