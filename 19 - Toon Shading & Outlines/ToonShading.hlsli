// Include guard
#ifndef __GGP_TOON_SHADING__
#define __GGP_TOON_SHADING__

// Defining several different methods for toon shading (including none at all)
#define TOON_SHADING_NONE			0
#define TOON_SHADING_RAMP			1
#define TOON_SHADING_CONDITIONALS	2


float ApplyToonShadingRamp(float originalNdotL, Texture2D rampTexture, SamplerState clampSampler)
{
	// Use the original N dot L (a value between 0 and 1) as a sampling
	// location in the ramp texture.  The red channel sampled from that
	// texture will be returned as the new N dot L value.
	return rampTexture.Sample(clampSampler, float2(originalNdotL, 0)).r;
}


float ApplyToonShadingDiffuseConditionals(float originalNdotL)
{
	// Check the original N dot L against a set of pre-defined values
	// Note: These were arbitrarily chosen values - you could have
	//       more of them or fewer of them, depending on the number
	//       of lighting "bands" you want the result to have
	if (originalNdotL < 0.1f) return 0.0f;
	if (originalNdotL < 0.45f) return 0.45f;
	if (originalNdotL < 0.8f) return 0.8f;

	// Anything else
	return 1.0f;
}

float ApplyToonShadingSpecularConditionals(float originalNdotL)
{
	// Check the original N dot L against a set of pre-defined values
	// Note: These were arbitrarily chosen values - you could have
	//       more of them or fewer of them, depending on the number
	//       of lighting "bands" you want the result to have
	if (originalNdotL < 0.8f) return 0.0f;

	// Anything else
	return 1.0f;
}


#endif