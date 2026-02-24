
// 3D value to 3D "random" vector
float3 random3D(float3 uv)
{
	return frac(sin(mul(uv, float3x3(0.129898, 0.78233, 0.11314, 0.15926, 0.54321, 0.98765, 0.90210, 0.1675309, 0.6789))) * 12346.6789);
}

// 3D value to 3D "random" unit vector
float3 randomUnitVector3D(float3 uv)
{
	return normalize(random3D(uv) - 0.5f);	
}

#define MAX_SPHERES 32

struct Sphere
{
	float3 Position;
	float Radius;
	float3 Color;
	float Roughness;
};

struct DrawData
{
	Sphere spheres[MAX_SPHERES];
	matrix invVP;
	float3 cameraPosition;
	uint sphereCount;
	float3 skyColor;
	uint windowWidth;
	uint windowHeight;
	uint maxRecursion;
	uint raysPerPixel;
};

struct Ray
{
	float3 Origin;
	float3 Direction;
};

struct HitDetails
{
	Sphere HitSphere;
	float3 HitPosition;
	bool Hit;
	float3 HitNormal;
	float HitDistance;
};

cbuffer DrawIndices : register(b0)
{
	uint cbIndex;
	uint outputTextureIndex;
}

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

bool RaySphereIntersect(Ray r, Sphere s, out float dist)
{
	// How far along ray to closest point to sphere center
	float3 originToCenter = s.Position - r.Origin;
	float tCenter = dot(originToCenter, r.Direction);

	// If tCenter is negative, we point away from sphere
	if (tCenter < 0)
	{
		dist = 0;
		return false;
	}
	
	// Distance from closest point to sphere's center
	float o2cLength = length(originToCenter);
	float d = sqrt(o2cLength * o2cLength - tCenter * tCenter);

	// If distance is greater than radius, we don't hit the sphere
	if (d > s.Radius)
	{
		dist = 0;
		return false;
	}
	
	// Offset from tCenter to an intersection point
	float offset = sqrt(s.Radius * s.Radius - d * d);

	// Distance to the hit point
	dist = tCenter - offset;
	return true;
}

bool TraceRay(Ray r, Sphere spheres[MAX_SPHERES], out HitDetails details)
{
	details.Hit = false;
	details.HitDistance = 999999.0f;

	for (int s = 0; s < MAX_SPHERES; s++)
	{
		float thisDist = 0;
		if (RaySphereIntersect(r, spheres[s], thisDist))
		{
			if (thisDist < details.HitDistance)
			{
				details.HitSphere = spheres[s];
				details.HitDistance = thisDist;
				details.Hit = true;
			}
		}
	}
	
	if(details.Hit)
	{
		details.HitPosition = r.Origin + normalize(r.Direction) * details.HitDistance;
		details.HitNormal = normalize(details.HitPosition - details.HitSphere.Position);
	}
	
	return details.Hit;
}


[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	// Grab draw data buffer
	ConstantBuffer<DrawData> cb = ResourceDescriptorHeap[cbIndex];

	
	// Set up the ray and trace
	float3 totalColor = float3(0,0,0);
	for(int r = 0; r < cb.raysPerPixel; r++)
	{
		Ray ray = GetRayThroughPixel(cb.cameraPosition, threadID.x, threadID.y, cb.windowWidth, cb.windowHeight, cb.invVP);
		
		float3 color = float3(1,1,1);
		for (int depth = 0; depth < cb.maxRecursion; depth++)
		{
			HitDetails details;
			if(!TraceRay(ray, cb.spheres, details))
			{
				color *= cb.skyColor;
				break;
			}
			else
			{
				color *= details.HitSphere.Color;	
			}
			
		
			// Origin for next trace
			ray.Origin = details.HitPosition + details.HitNormal * 0.001f;
		
			// Check roughness
			if (details.HitSphere.Roughness == 0)
			{
				// Mirror
				ray.Direction = reflect(ray.Direction, details.HitNormal);
			}
			else
			{
				// Diffuse
				float3 rand = randomUnitVector3D(details.HitPosition + r);
				if (dot(rand, details.HitNormal) < 0) 
					rand *= -1;
				
				ray.Direction = rand;
			}
		}
		
		totalColor += color;
	}
	
	totalColor /= cb.raysPerPixel;
	
	// Write final color to RW Texture
	RWTexture2D<unorm float4> Output = ResourceDescriptorHeap[outputTextureIndex];
	Output[threadID.xy] = float4(pow(totalColor, 1.0f/2.2f), 1);
}