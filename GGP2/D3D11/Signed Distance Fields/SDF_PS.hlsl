
#define EPSILON 0.001f

#define SDF_TYPE_SPHERE		0
#define SDF_TYPE_BOX		1
#define SDF_TYPE_BOX_ROUND	2
#define SDF_TYPE_PLANE		3
#define SDF_TYPE_COMPLEX	4

// Shape inputs:
// Sphere - pos, radius
// Box - pos, bounds (float3)
// Plane - pos, normal, d (float)

struct Shape
{
	uint Type;			// Common
	float3 Position;	// Common
	// ---
	float3 Color;		// Common	
	float Radius;		// Sphere or rounded box
	// ---
	float3 Bounds;		// Box
	float D;			// Plane
	// ---
	float3 Normal;		// Plane
	float pad;
};

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


#define MAX_SHAPES 10
cbuffer ExternalData : register(b0)
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
}



// Calculates an origin and direction from the camera for specific pixel indices
Ray CalcRayFromCamera(float2 rayIndices)
{
	// Offset to the middle of the pixel
	float2 pixel = rayIndices + 0.5f;
	float2 screenPos = pixel / float2(screenWidth, screenHeight) * 2.0f - 1.0f;
	screenPos.y = -screenPos.y;

	// Unproject the coords
	float4 worldPos = mul(inverseViewProjection, float4(screenPos, 0, 1));
	worldPos.xyz /= worldPos.w;

	// Set up the ray
	Ray ray;
	ray.Origin = cameraPosition;
	ray.Direction = normalize(worldPos.xyz - ray.Origin);
	return ray;
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

HitResult UnionHit(HitResult a, HitResult b)
{
	if(a.Dist < b.Dist) 
		return a;
	else
		return b;
}

float SubtractionSDF(float a, float b)
{
	return max(-a, b);	
}

HitResult Scene(float3 samplePos, Shape shapes[MAX_SHAPES], int shapeCount)
{
	HitResult finalHit;
	finalHit.Dist = 999;
	finalHit.Color = float3(0,0,0);
	
	for(int i = 0; i < shapeCount; i++)
	{
		HitResult hit;
		hit.Dist = 999;
		hit.Color = shapes[i].Color;
		switch (shapes[i].Type)
		{
			case SDF_TYPE_SPHERE: hit.Dist = SphereSDF(samplePos, shapes[i].Position, shapes[i].Radius); break;
			case SDF_TYPE_BOX: hit.Dist = BoxSDF(samplePos, shapes[i].Position, shapes[i].Bounds); break;
			case SDF_TYPE_BOX_ROUND: hit.Dist = BoxRoundSDF(samplePos, shapes[i].Position, shapes[i].Bounds, shapes[i].Radius); break;
			case SDF_TYPE_PLANE: hit.Dist = PlaneSDF(samplePos, shapes[i].Position, shapes[i].Normal, shapes[i].D); break;
			case SDF_TYPE_COMPLEX:
				hit.Dist = SubtractionSDF(
					BoxRoundSDF(samplePos, float3(-10, 5, 5), float3(4,1,1), 0.5f), 
					SphereSDF(samplePos, float3(-10, 5, 5), 3.0f));
				
				hit.Dist = SubtractionSDF(
					BoxRoundSDF(samplePos, float3(-10, 5, 5), float3(1,4,1), 0.5f),
					hit.Dist);
			
				hit.Dist = SubtractionSDF(
					BoxRoundSDF(samplePos, float3(-10, 5, 5), float3(1,1,4), 0.5f),
					hit.Dist);
			
				break;
		}
		
		finalHit = UnionHit(finalHit, hit);
	}
	
	return finalHit;
}

float3 NormalSDF_3_Sample(float3 samplePos, float d, Shape shapes[MAX_SHAPES], int shapeCount)
{
	return normalize(float3(
		Scene(samplePos + float3(EPSILON, 0, 0), shapes, shapeCount).Dist - d,
		Scene(samplePos + float3(0, EPSILON, 0), shapes, shapeCount).Dist - d,
		Scene(samplePos + float3(0, 0, EPSILON), shapes, shapeCount).Dist - d));
}

float3 GetSkyColor(float3 dir)
{
	float3 skyUp = float3(0.5f, 0.8f, 1);
	float3 skyDown = float3(0.1f, 0.2f, 1);
	
	return lerp(skyDown, skyUp, saturate(dot(float3(0,1,0), normalize(dir)) * 0.5f + 0.5f));
}


float RayMarchShadow(Ray ray, Shape shapes[MAX_SHAPES], int shapeCount, float w)
{
	float tmax = 50.0f;
	
	float res = 1.0;
    float t = EPSILON;
    for( int i=0; i<64 && t<tmax; i++ )
    {
        float h = Scene(ray.Origin + ray.Direction * t, shapes, shapeCount).Dist;
		float s = clamp(8.0*h/(w*t),0.0,1.0);
        res = min( res, s );
        t += clamp( h, 0.01, 0.2 );
        if( res<0.004 || t>tmax ) break;
    }
    res = clamp( res, 0.0, 1.0 );
    return res*res*(3.0-2.0*res);
}

float3 RayMarch(Ray ray, Shape shapes[MAX_SHAPES], int shapeCount)
{
	float3 color = float3(1,1,1);
	float3 normal = float3(0,0,0);
	float3 refl = float3(0,0,0);
	
	float3 startPos = cameraPosition;
	
	// Handle reflections in a loop (first iteration is base color)
	for(int reflIndex = 0; reflIndex <= reflectionCount; reflIndex++)
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
			HitResult hit = Scene(currPosition, shapes, shapeCount);
			if (hit.Dist < EPSILON)
			{
				anyHit = true;
				
				// Calculate normal here (more ray marching of the scene)
				normal = NormalSDF_3_Sample(currPosition, hit.Dist, shapes, shapeCount);
			
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
					diffuse *= RayMarchShadow(rayTowardsLight, shapes, shapeCount, shadowSpread);
				}
				
				// Specular (Phong)
				float spec = pow(max(0, dot(-ray.Direction, lightRefl)), 32.0f);
				spec *= diffuse;
				
				// Ambient (hemispheric ambient)
				float3 ambient = GetSkyColor(normal) * ambientAmount;
			
				// Combine
				color *= (hit.Color * diffuse + spec + ambient);
				
				break;
			}
		
			// Step forward
			currentDist += hit.Dist;
		
			// Too far, exit early
			if (currentDist >= 100)
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


float4 main(float4 position	: SV_POSITION) : SV_TARGET
{
	Shape shapes[10];
	int shapeCount = 6;
	
	shapes[0].Type = SDF_TYPE_SPHERE;
	shapes[0].Position = float3(-2,1,0);
	shapes[0].Radius = 1.0f;
	shapes[0].Color = float3(1,0.75f,0.25f);
	
	shapes[1].Type = SDF_TYPE_SPHERE;
	shapes[1].Position = float3(3,2,0);
	shapes[1].Radius = 2.0f;
	shapes[1].Color = float3(0.25f, 1, 0.75f);
	
	shapes[2].Type = SDF_TYPE_BOX;
	shapes[2].Position = float3(0,6,5);
	shapes[2].Bounds = float3(5, 5, 2);
	shapes[2].Color = float3(0.75f, 0.25f, 1);
	
	shapes[3].Type = SDF_TYPE_PLANE;
	shapes[3].Position = float3(0,0,0);
	shapes[3].Normal = float3(0,1,0);
	shapes[3].D = 0.0f;
	shapes[3].Color = float3(0.5f, 0.5f, 0.5f);
	
	shapes[4].Type = SDF_TYPE_BOX_ROUND;
	shapes[4].Position = float3(11, 6, 5);
	shapes[4].Bounds = float3(5, 5, 2);
	shapes[4].Radius = 1.0f;
	shapes[4].Color = float3(1.0f, 0.2f, 0.2f);
	
	shapes[5].Type = SDF_TYPE_COMPLEX;
	shapes[5].Color = float3(1.0f, 0.2f, 0.2f);
	
	// Ray from camera through pixel into scene
	Ray ray = CalcRayFromCamera(position.xy);
	
	// Ray march the scene to get a color for this pixel
	float3 color = RayMarch(ray, shapes, shapeCount);
	
	// Gamma correction
	return float4(pow(color, 1.0f / 2.2f), 1);
	
}