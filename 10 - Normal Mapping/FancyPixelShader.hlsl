
cbuffer ExternalData : register(b0)
{
	float3 colorTint;
	float time;
}

// Struct representing the data we expect to receive from earlier pipeline stages
struct VertexToPixel
{
	float4 screenPosition	: SV_POSITION;
	float2 uv				: TEXCOORD;
	float3 normal			: NORMAL;
};

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	// Start with a black canvas
	float4 returnColor = float4(0,0,0,0);

	// Change the V tex coord to the -1 to 1 space, then scale
	float heightScale = 10.0f;
	float v = (input.uv.y * -2 + 1) * heightScale;

	// Value for sin waves
	const int NUM_WAVES = 3;
	float speed[NUM_WAVES] = { 7.0f, 10.0f, 6.0f };
	float freq[NUM_WAVES] = { 3.0f, 2.0f, 1.0f };
	float amp[NUM_WAVES] = { 2.0f, 0.75f, 3.0f };
	float glow[NUM_WAVES] = { 1.0f, 0.5f, 2.0f };
	float intensities[NUM_WAVES] = { 5.0f, 20.0f, 10.0f };
	float3 colors[NUM_WAVES] = {
		float3(1.0f, 0.1f, 0.1f),
		float3(0.1f, 1.0f, 0.1f),
		float3(0.0f, 0.1f, 1.0f)
	};

	float3 total = 0.0f;
	for (int i = 0; i < NUM_WAVES; i++)
	{
		// Calculate a sin wave based on the U tex coord, frequency, time and amplitude
		float f = freq[i] * 3.14159f * 2.0f; // Multiple of 2pi for wrapping
		float s = sin(input.uv.x * f + time * speed[i] + sin(time)) * amp[i];

		// How far from the sin wave are we?
		float dist = 1.0f - saturate(pow(abs(v - s), glow[i]));
		total += colors[i] * dist * intensities[i];
	}

	return float4(colorTint * total, 1);
}