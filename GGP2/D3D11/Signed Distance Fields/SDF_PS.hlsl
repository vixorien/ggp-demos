
#define SDF_TYPE_SPHERE		0
#define SDF_TYPE_BOX		1
#define SDF_TYPE_PLANE		2

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
	float Radius;		// Sphere
	// ---
	float3 Bounds;		// Box
	float D;			// Plane
	// ---
	float3 Normal;		// Plane
	float pad;
};

#define MAX_SHAPES 10
cbuffer ExternalData : register(b0)
{
	matrix inverseViewProjection;
	float3 cameraPosition;
	
	int screenWidth;
	int screenHeight;
	
	//int shapeCount;
	//Shape shapes[MAX_SHAPES];
}

struct Ray
{
	float3 Origin;
	float3 Direction;
};

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

struct Hit
{
	float Dist;
	float3 Color;
};

float SphereSDF(float3 samplePos, float3 spherePos, float radius)
{
	return length(samplePos - spherePos) - radius;	
}

float BoxSDF(float3 samplePos, float3 boxPos, float3 boxBounds)
{
	float3 diff = abs(samplePos - boxPos) - boxBounds;
	return length(max(diff, 0.0f)) + min(max(diff.x, max(diff.y, diff.z)), 0.0f);
}

float PlaneSDF(float3 samplePos, float3 planePos, float3 normal, float d)
{
	return dot(samplePos - planePos, normal) + d;
}

Hit UnionSDF(Hit a, Hit b)
{
	if(a.Dist < b.Dist) 
		return a;
	
	return b;
}

Hit Scene(float3 samplePos, Shape shapes[MAX_SHAPES], int shapeCount)
{
	Hit finalHit;
	finalHit.Dist = 999;
	finalHit.Color = float3(0,0,0);
	
	for(int i = 0; i < shapeCount; i++)
	{
		Hit hit;
		hit.Color = shapes[i].Color;
		switch (shapes[i].Type)
		{
			case SDF_TYPE_SPHERE: hit.Dist = SphereSDF(samplePos, shapes[i].Position, shapes[i].Radius); break;
			case SDF_TYPE_BOX: hit.Dist = BoxSDF(samplePos, shapes[i].Position, shapes[i].Bounds); break;
			case SDF_TYPE_PLANE: break;
		}
		
		finalHit = UnionSDF(finalHit, hit);
	}
	
	return finalHit;
}


#define EPSILON 0.000001f

float4 main(float4 position	: SV_POSITION) : SV_TARGET
{
	Shape shapes[10];
	
	shapes[0].Type = SDF_TYPE_SPHERE;
	shapes[0].Position = float3(0,0,0);
	shapes[0].Radius = 1.0f;
	shapes[0].Color = float3(1,0,0);
	
	shapes[1].Type = SDF_TYPE_SPHERE;
	shapes[1].Position = float3(3,0,0);
	shapes[1].Radius = 2.0f;
	shapes[1].Color = float3(0, 1, 0);
	
	shapes[2].Type = SDF_TYPE_BOX;
	shapes[2].Position = float3(-5,0,0);
	shapes[2].Bounds = float3(2, 5, 2);
	shapes[2].Color = float3(0, 0, 1);
	
	Ray ray = CalcRayFromCamera(position.xy);
	
	float currentDist = 0;
	
	for(int i = 0; i < 256; i++)
	{
		Hit hit = Scene(cameraPosition + ray.Direction * currentDist, shapes, 3);
		if(hit.Dist < EPSILON)
		{
			return hit.Color.rgbb;
		}
		
		currentDist += hit.Dist;
		
		if(currentDist >= 100)
			return float4(0,0,0,0);
	}
	
	return float4(0,0,0,0);
	
}