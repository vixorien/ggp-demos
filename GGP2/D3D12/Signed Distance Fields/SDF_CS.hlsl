
#define EPSILON 0.001f

#define MAX_SCENE_BOUNDS 100.0f

struct HitResult
{
	float Dist;
	float3 Color;
};

struct Ray
{
	float3 Origin;
	float3 Direction;
};

struct DrawData
{
	matrix inverseViewProjection;
	float3 cameraPosition;
	
	int screenWidth;
	int screenHeight;
	
	float totalTime;
	
	int reflectionCount;
	float ambientAmount;
	int shadows;
	float shadowSpread;
};


cbuffer DrawIndices : register(b0)
{
	uint cbIndex;
	uint outputTextureIndex;
	uint skyboxIndex;
}

SamplerState BasicSampler : register(s0);

Ray GetRayThroughPixel(float3 camPos, float x, float y, float width, float height, matrix invVP)
{
	// Calculate NDCs
	float4 ndc = float4(x, y, 0, 1);
	ndc.x = ndc.x / width * 2 - 1;
	ndc.y = ndc.y / height * 2 - 1;
	ndc.y = -ndc.y;
	
	// Unproject
	float4 unproj = mul(invVP, ndc);
	unproj /= unproj.w;
	
	// Finalize ray
	Ray ray;
	ray.Origin = camPos;
	ray.Direction = normalize(unproj.xyz - ray.Origin);
	return ray;
}

// === Misc light functions ===

float3 GetSkyColor(float3 dir)
{
	TextureCube sky = ResourceDescriptorHeap[skyboxIndex];
	return sky.SampleLevel(BasicSampler, dir, 0).rgb;
}

float3 GetCheckerColor(float2 pos, float scale, float3 colorA, float3 colorB)
{
	float fx = frac(pos.x / scale);
	float fy = frac(pos.y / scale);
	return lerp(colorA, colorB, fy < 0.5f ? fx < 0.5f : fx >= 0.5f);
}

float FresnelSchlick(float f0, float3 n, float3 v)
{
	return f0 + (1-f0) * pow(1 - saturate(dot(n,v)), 5.0f);
}

// === SDF functions ===

float SphereSDF(float3 samplePos, float3 spherePos, float radius)
{
	return length(samplePos - spherePos) - radius;	
}

float BoxSDF(float3 samplePos, float3 boxPos, float3 boxBounds)
{
	float3 diff = abs(samplePos - boxPos) - boxBounds;
	return length(max(diff, 0.0f)) + min(max(diff.x, max(diff.y, diff.z)), 0.0f);
}

float BoxRoundSDF(float3 samplePos, float3 boxPos, float3 boxBounds, float radius)
{
	float3 diff = abs(samplePos - boxPos) - boxBounds + radius;
	return length(max(diff, 0.0f)) + min(max(diff.x, max(diff.y, diff.z)), 0.0f) - radius;
}

float PlaneSDF(float3 samplePos, float3 planePos, float3 normal, float d)
{
	return dot(samplePos - planePos, normal) + d;
}


// === Combination Functions ===

float UnionSDF(float a, float b)
{
	return min(a, b);	
}

float SubtractionSDF(float a, float b)
{
	return max(-a, b);	
}

float IntersectionSDF(float a, float b)
{
	return max(a, b);	
}

// Same as UnionSDF, but works w/ struct
HitResult UnionHit(HitResult a, HitResult b)
{
	if(a.Dist < b.Dist) 
		return a;
	else
		return b;
}


// === Scene evaluation ===

HitResult EvaluateScene(float3 samplePos, float time)
{
	// Best hit and current hit we're checking
	HitResult bestHit;
	HitResult hit;
	
	// Initial floor plane
	bestHit.Dist = PlaneSDF(samplePos, float3(0,0,0), float3(0,1,0), 0.0f);
	bestHit.Color = GetCheckerColor(samplePos.xz, 5.0f, 0.75f, 0.45f);
	
	// First sphere
	hit.Dist = SphereSDF(samplePos, float3(sin(time) * 3.0f, 2, 0), 1.0f);
	hit.Color = float3(1, 0.75f, 0.25f);
	bestHit = UnionHit(bestHit, hit);
	
	// Second sphere
	hit.Dist = SphereSDF(samplePos, float3(3, 4, sin(time * 1.5f) * 3.0f), 2.0f);
	hit.Color = float3(0.25f, 1, 0.75f);
	bestHit = UnionHit(bestHit, hit);
	
	// Box
	hit.Dist = BoxSDF(samplePos, float3(0,6,5), float3(5, 5, 2));
	hit.Color = float3(0.75f, 0.25f, 1);
	bestHit = UnionHit(bestHit, hit);
	
	// Rounded box
	hit.Dist = BoxRoundSDF(samplePos, float3(11, 6, 5), float3(5, 5, 2), 1.0f);
	hit.Color = float3(1.0f, 0.2f, 0.2f);
	bestHit = UnionHit(bestHit, hit);
	
	// Complex shape
	hit.Color = float3(1.0f, 0.2f, 0.2f);
	hit.Dist = SphereSDF(samplePos, float3(-10, 5, 5), 3.0f);
	hit.Dist = SubtractionSDF(BoxRoundSDF(samplePos, float3(-10, 5, 5), float3(4,1,1), 0.5f), hit.Dist);
	hit.Dist = SubtractionSDF(BoxRoundSDF(samplePos, float3(-10, 5, 5), float3(1,4,1), 0.5f), hit.Dist);
	hit.Dist = SubtractionSDF(BoxRoundSDF(samplePos, float3(-10, 5, 5), float3(1,1,4), 0.5f), hit.Dist);
	bestHit = UnionHit(bestHit, hit);
	
	// Another complex shape
	hit.Color = float3(1.0f, 1.0f, 0.2f);
	hit.Dist = BoxRoundSDF(samplePos, float3(10, 5, 0), float3(2,2,1), 0.25f);
	hit.Dist = IntersectionSDF(SphereSDF(samplePos, float3(10, 6, 0), 2.0f), hit.Dist);
	bestHit = UnionHit(bestHit, hit);
	
	return bestHit;
}

// === Surface normal ===

float3 NormalSDF_3_Sample(float3 samplePos, float d, float time)
{
	return normalize(float3(
		EvaluateScene(samplePos + float3(EPSILON, 0, 0), time).Dist - d,
		EvaluateScene(samplePos + float3(0, EPSILON, 0), time).Dist - d,
		EvaluateScene(samplePos + float3(0, 0, EPSILON), time).Dist - d));
}

// === Soft shadow evaluation ===

float RayMarchShadow(Ray ray, float w, float tMax, float time)
{
	// Starting values
	float lightAmount = 1.0;
    float t = EPSILON;
	
	// Ray march, stopping if we go too far
    for( int i = 0; i < 64 && t < tMax; i++ )
    {
		// Get distance of the scene
        float h = EvaluateScene(ray.Origin + ray.Direction * t, time).Dist;
		
		// Distance to closest surface, and step forward
		float s = clamp(8.0f * h / (w * t), 0.0f, 1.0f);
		t += clamp(h, 0.01, 0.2);
		
		// Reduce light and exit if small enough
		lightAmount = min(lightAmount, s);
		if (lightAmount < EPSILON) 
			break;
    }
    
	// Smooth results with a clamped lightAmount
	return smoothstep(0, 1, saturate(lightAmount));
}

// === Ray marching through scene ===

float3 RayMarch(Ray ray, float3 camPos, float time, int reflCount, int shadows, float shadowSpread, float ambientAmount)
{
	float3 color = float3(1,1,1);
	float3 refl = float3(0,0,0);
	
	float3 startPos = camPos;
	
	// Handle reflections in a loop (first iteration is base color)
	for(int reflIndex = 0; reflIndex <= reflCount; reflIndex++)
	{
		float currentDist = 0;
		float3 currPosition = 0;
		
		// Ray march
		bool anyHit = false;
		for (int i = 0; i < 256; i++)
		{
			// Calculate new position
			currPosition = startPos + ray.Direction * currentDist;
		
			// Ray march the scene
			HitResult hit = EvaluateScene(currPosition, time);
			if (hit.Dist < EPSILON)
			{
				anyHit = true;
				
				// Calculate normal here (more ray marching of the scene)
				float3 normal = NormalSDF_3_Sample(currPosition, hit.Dist, time);
			
				// Reflection ray (for next bounce after loop)
				refl = reflect(ray.Direction, normal);
			
				// Light details
				float3 lightDir = normalize(float3(0.75f,-1,0.8));
				float3 lightRefl = reflect(lightDir, normal);
				
				// Diffuse (lambert)
				float diffuse = max(dot(normal, -lightDir), 0);
				
				// Shadows (ray march again towards light)
				if(shadows)
				{
					Ray rayTowardsLight;
					rayTowardsLight.Direction = -lightDir;
					rayTowardsLight.Origin = currPosition;
					diffuse *= RayMarchShadow(rayTowardsLight, shadowSpread, MAX_SCENE_BOUNDS, time);
				}
				
				// Specular (Phong)
				float specular = pow(max(0, dot(-ray.Direction, lightRefl)), 32.0f);
				specular *= FresnelSchlick(0.04f, normal, normalize(camPos - currPosition));
				specular *= 3.0f; // Arbitrary specular constant (k)
				specular *= diffuse; // Limit based on lambert & shadows, too
				
				// Ambient (hemispheric ambient)
				float3 ambient = GetSkyColor(normal) * ambientAmount * hit.Color;
			
				// Combine
				color *= (hit.Color * diffuse + specular + ambient);
				
				break;
			}
		
			// Step forward
			currentDist += hit.Dist;
		
			// Too far, exit early
			if (currentDist >= MAX_SCENE_BOUNDS)
				break;
		}
		
		// No hits, use sky color
		if(!anyHit)
		{
			color *= GetSkyColor(ray.Direction);
			break;
		}
		
		// New position and direction based on reflection
		ray.Direction = refl;
		startPos = currPosition + ray.Direction * 0.01f;
	}
	
	return color;
}


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	// Grab draw data buffer
	ConstantBuffer<DrawData> cb = ResourceDescriptorHeap[cbIndex];

	// Ray from camera through pixel into scene
	Ray ray = GetRayThroughPixel(cb.cameraPosition, threadID.x, threadID.y, cb.screenWidth, cb.screenHeight, cb.inverseViewProjection);
		
	// Ray march the scene to get a color for this pixel
	float3 color = RayMarch(ray, cb.cameraPosition, cb.totalTime, cb.reflectionCount, cb.shadows, cb.shadowSpread, cb.ambientAmount);
	
	// Write final color to RW Texture
	RWTexture2D<unorm float4> Output = ResourceDescriptorHeap[outputTextureIndex];
	Output[threadID.xy] = float4(pow(color, 1.0f/2.2f), 1);
}