#ifndef __GGP_LIGHTING__
#define __GGP_LIGHTING__

// Basic lighting functions, constants and other useful structures

#define MAX_LIGHTS 32 // Make sure this matches C++!

#define MAX_SPECULAR_EXPONENT 256.0f

#define LIGHT_TYPE_DIRECTIONAL	0
#define LIGHT_TYPE_POINT		1
#define LIGHT_TYPE_SPOT			2

struct Light
{
	int		Type;
	float3	Direction;	// 16 bytes

	float	Range;
	float3	Position;	// 32 bytes

	float	Intensity;
	float3	Color;		// 48 bytes

	float	SpotInnerAngle;
	float	SpotOuterAngle;
	float2	Padding;	// 64 bytes
};


// === CONSTANTS ====================================================

static const float PI = 3.14159265359f;
static const float TWO_PI = PI * 2.0f;
static const float HALF_PI = PI / 2.0f;
static const float QUARTER_PI = PI / 4.0f;


// === UTILITY FUNCTIONS ============================================

// Range-based attenuation function
float Attenuate(Light light, float3 worldPos)
{
	// Calculate the distance between the surface and the light
	float dist = distance(light.Position, worldPos);

	// Ranged-based attenuation
	float att = saturate(1.0f - (dist * dist / (light.Range * light.Range)));

	// Soft falloff
	return att * att;
}


// === BASIC LIGHTING ===============================================

// Lambert diffuse BRDF
float Diffuse(float3 normal, float3 dirToLight)
{
	return saturate(dot(normal, dirToLight));
}


// Phong (specular) BRDF
float SpecularPhong(float3 normal, float3 dirToLight, float3 toCamera, float roughness)
{
	// Calculate reflection vector
	float3 refl = reflect(-dirToLight, normal);

	// Compare reflection vector & view vector and raise to a power
	return roughness == 1 ? 0.0f : pow(max(dot(toCamera, refl), 0), (1 - roughness) * MAX_SPECULAR_EXPONENT);
}


// Blinn-Phong (specular) BRDF
float SpecularBlinnPhong(float3 normal, float3 dirToLight, float3 toCamera, float roughness)
{
	// Calculate halfway vector
	float3 halfwayVector = normalize(dirToLight + toCamera);

	// Compare halflway vector & normal and raise to a power
	return roughness == 1 ? 0.0f : pow(max(dot(halfwayVector, normal), 0), (1 - roughness) * MAX_SPECULAR_EXPONENT);
}




// === LIGHT TYPES FOR BASIC LIGHTING ===============================

float3 DirLight(Light light, float3 normal, float3 worldPos, float3 camPos, float roughness, float3 surfaceColor, float specularScale)
{
	// Get normalize direction to the light
	float3 toLight = normalize(-light.Direction);
	float3 toCam = normalize(camPos - worldPos);

	// Calculate the light amounts
	float diff = Diffuse(normal, toLight);
	float spec = SpecularPhong(normal, toLight, toCam, roughness) * specularScale;

	// Combine
	return (diff * surfaceColor + spec) * light.Intensity * light.Color;
}


float3 PointLight(Light light, float3 normal, float3 worldPos, float3 camPos, float roughness, float3 surfaceColor, float specularScale)
{
	// Calc light direction
	float3 toLight = normalize(light.Position - worldPos);
	float3 toCam = normalize(camPos - worldPos);

	// Calculate the light amounts
	float atten = Attenuate(light, worldPos);
	float diff = Diffuse(normal, toLight);
	float spec = SpecularPhong(normal, toLight, toCam, roughness) * specularScale;

	// Combine
	return (diff * surfaceColor + spec) * atten * light.Intensity * light.Color;
}


float3 SpotLight(Light light, float3 normal, float3 worldPos, float3 camPos, float roughness, float3 surfaceColor, float specularScale)
{
	// Calculate the spot falloff
	float3 toLight = normalize(light.Position - worldPos);
	float pixelAngle = saturate(dot(-toLight, light.Direction));
	
	// Our spot angle is really the cosine of the angle due
	// to the dot product, so we need the cosine of these angles, too
	float cosOuter = cos(light.SpotOuterAngle);
	float cosInner = cos(light.SpotInnerAngle);
	float falloffRange = cosOuter - cosInner;
	
	// Linear falloff as the angle to the pixel gets closer to the outer range
	float spotTerm = saturate((cosOuter - pixelAngle) / falloffRange);

	// Combine with the point light calculation
	// Note: This could be optimized a bit!  Doing a lot of the same work twice!
	return PointLight(light, normal, worldPos, camPos, roughness, surfaceColor, specularScale) * spotTerm;
}

#endif