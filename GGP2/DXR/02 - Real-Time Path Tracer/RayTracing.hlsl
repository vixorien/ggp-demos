

// === Defines ===

#define PI 3.141592654f

// === Structs ===

// Layout of data in the vertex buffer
struct Vertex
{
    float3 localPosition;
    float2 uv;
    float3 normal;
    float3 tangent;
};

struct SceneData
{
	matrix InverseViewProjection;
	float3 CameraPosition;
	uint RaysPerPixel;
};

struct EntityData
{
	float4 Color;
	uint VertexBufferDescriptorIndex;
	uint IndexBufferDescriptorIndex;
	float pad[2];
};

// Payload for rays (data that is "sent along" with each ray during raytrace)
// Note: This should be as small as possible, and must match our C++ size definition
struct RayPayload
{
	float3 Color;
	uint RecursionDepth;
	uint RayPerPixelIndex;
};

// Note: We'll be using the built-in BuiltInTriangleIntersectionAttributes struct
// for triangle attributes, so no need to define our own.  It contains a single float2.



// === Constant buffers ===

cbuffer DrawData : register(b0)
{
	uint SceneDataConstantBufferIndex;
	uint EntityDataDescriptorIndex;
	uint SceneTLASDescriptorIndex;
	uint OutputUAVDescriptorIndex;
};


// === Helpers ===


// Barycentric interpolation of data from the triangle's vertices
Vertex InterpolateVertices(uint triangleIndex, float2 barycentrics)
{
	// Get the data for this entity
	StructuredBuffer<EntityData> ed = ResourceDescriptorHeap[EntityDataDescriptorIndex];
	EntityData thisEntity = ed[InstanceIndex()];
	
	// Get the geometry buffers
	StructuredBuffer<uint> IndexBuffer    = ResourceDescriptorHeap[thisEntity.IndexBufferDescriptorIndex];
	StructuredBuffer<Vertex> VertexBuffer = ResourceDescriptorHeap[thisEntity.VertexBufferDescriptorIndex];

	// Grab the 3 indices for this triangle
	uint firstIndex = triangleIndex * 3;
	uint indices[3];
	indices[0] = IndexBuffer[firstIndex + 0];
	indices[1] = IndexBuffer[firstIndex + 1];
	indices[2] = IndexBuffer[firstIndex + 2];

	// Grab the 3 corresponding vertices
	Vertex verts[3];
	verts[0] = VertexBuffer[indices[0]];
	verts[1] = VertexBuffer[indices[1]];
	verts[2] = VertexBuffer[indices[2]];
	
	// Calculate the barycentric data for vertex interpolation
	float3 barycentricData = float3(
		1.0f - barycentrics.x - barycentrics.y,
		barycentrics.x,
		barycentrics.y);
	
	// Loop through the barycentric data and interpolate
    Vertex finalVert = (Vertex)0;
	for (uint i = 0; i < 3; i++)
	{
		finalVert.localPosition += verts[i].localPosition * barycentricData[i];
		finalVert.uv += verts[i].uv * barycentricData[i];
		finalVert.normal += verts[i].normal * barycentricData[i];
		finalVert.tangent += verts[i].tangent * barycentricData[i];
	}
	return finalVert;
}


// Calculates an origin and direction from the camera for specific pixel indices
RayDesc CalcRayFromCamera(float2 rayIndices, float3 camPos, float4x4 invVP)
{
	// Offset to the middle of the pixel
	float2 pixel = rayIndices + 0.5f;
	float2 screenPos = pixel / DispatchRaysDimensions().xy * 2.0f - 1.0f;
	screenPos.y = -screenPos.y;

	// Unproject the coords
	float4 worldPos = mul(invVP, float4(screenPos, 0, 1));
	worldPos.xyz /= worldPos.w;

	// Set up the ray
	RayDesc ray;
	ray.Origin = camPos.xyz;
	ray.Direction = normalize(worldPos.xyz - ray.Origin);
	ray.TMin = 0.01f;
	ray.TMax = 1000.0f;
	return ray;
}


// === Pseudo-Random Number Generation ===

// Based on https://thebookofshaders.com/10/
float rand(float2 uv)
{
	return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float2 rand2(float2 uv)
{
	return float2(
		rand(uv),
		rand(uv.yx));
}

// From: https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg_hash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float pcg_hash_0_1(uint input)
{
	return (float)pcg_hash(input) / (-1u);	
}


// === Sampling Functions ===
// Based on Chapter 16 of Raytracing Gems

// Params should be uniform between [0,1]
float3 RandomVector(float u0, float u1)
{
	float a = u0 * 2 - 1;
	float b = sqrt(1 - a * a);
	float phi = 2.0f * PI * u1;

	float x = b * cos(phi);
	float y = b * sin(phi);
	float z = a;

	return float3(x, y, z);
}

// First two params should be uniform between [0,1]
float3 RandomCosineWeightedHemisphere(float u0, float u1, float3 unitNormal)
{
	float a = u0 * 2 - 1;
	float b = sqrt(1 - a * a);
	float phi = 2.0f * PI * u1;

	float x = unitNormal.x + b * cos(phi);
	float y = unitNormal.y + b * sin(phi);
	float z = unitNormal.z + a;

	// float pdf = a / PI;
	return float3(x, y, z);
}



// === Shaders ===

// Ray generation shader - Launched once for each ray we want to generate
// (which is generally once per pixel of our output texture)
[shader("raygeneration")]
void RayGen()
{
	// Grab the constant buffer
	ConstantBuffer<SceneData> cb = ResourceDescriptorHeap[SceneDataConstantBufferIndex];
	
	// Get the ray indices
	uint2 rayIndices = DispatchRaysIndex().xy;
	
	// Average of all rays per pixel
	float3 totalColor = float3(0, 0, 0);
	
	for (int r = 0; r < cb.RaysPerPixel; r++)
	{
		float2 adjustedIndices = (float2) rayIndices;
		float ray01 = (float) r / cb.RaysPerPixel;
		adjustedIndices += rand2(rayIndices.xy * ray01);

		// Calculate the ray from the camera through a particular
		// pixel of the output buffer using this shader's indices
		RayDesc ray = CalcRayFromCamera(
			rayIndices,
			cb.CameraPosition,
			cb.InverseViewProjection);

		// Set up the payload for the ray
		// This initializes the struct to all zeros
		RayPayload payload = (RayPayload) 0;
		payload.Color = float3(1, 1, 1);
		payload.RecursionDepth = 0;
		payload.RayPerPixelIndex = r;

		// Perform the ray trace for this ray
		RaytracingAccelerationStructure SceneTLAS = ResourceDescriptorHeap[SceneTLASDescriptorIndex];
		TraceRay(
			SceneTLAS,
			RAY_FLAG_NONE,
			0xFF,
			0, 0, 0,
			ray,
			payload);
		
		totalColor += payload.Color;
	}

	// Set the final color of the buffer (gamma corrected)
	RWTexture2D<float4> OutputColor = ResourceDescriptorHeap[OutputUAVDescriptorIndex];
	OutputColor[rayIndices] = float4(pow(totalColor / cb.RaysPerPixel, 1.0f / 2.2f), 1);
}


// Miss shader - What happens if the ray doesn't hit anything?
[shader("miss")]
void Miss(inout RayPayload payload)
{
	// Hemispheric gradient
	float3 upColor = float3(0.3f, 0.5f, 0.95f);
	float3 downColor = float3(1, 1, 1);

	// Interpolate based on the direction of the ray
	float interpolation = dot(normalize(WorldRayDirection()), float3(0, 1, 0)) * 0.5f + 0.5f;
	float3 skyColor = lerp(downColor, upColor, interpolation);
	
	// Alter the payload color by the sky color
	payload.Color *= skyColor;
}


// Closest hit shader - Runs the first time a ray hits anything
[shader("closesthit")]
void ClosestHit(inout RayPayload payload, BuiltInTriangleIntersectionAttributes hitAttributes)
{
	// If we've reached the max recursion, we haven't hit a light source (the sky, which is the "miss shader" here)
	if (payload.RecursionDepth == 10)
	{
		payload.Color = float3(0, 0, 0);
		return;
	}
	
	// Get the data for this entity
	StructuredBuffer<EntityData> entityDataBuffer = ResourceDescriptorHeap[EntityDataDescriptorIndex];
	EntityData thisEntity = entityDataBuffer[InstanceIndex()];
	
	// We've hit, so adjust the payload color by this instance's color
	payload.Color *= thisEntity.Color.rgb;
	
	// Get the geometry hit details and convert normal to world space
	Vertex hit = InterpolateVertices(PrimitiveIndex(), hitAttributes.barycentrics);
	float3 normal_WS = normalize(mul(hit.normal, (float3x3)ObjectToWorld4x3()));
	
	// Calc a unique RNG value for this ray, based on the "uv" (0-1 location) of this pixel and other per-ray data
	float2 pixelUV = (float2)DispatchRaysIndex().xy / DispatchRaysDimensions().xy;
	float2 rng = rand2(pixelUV * (payload.RecursionDepth + 1) + payload.RayPerPixelIndex + RayTCurrent());
	
	// Interpolate between perfect reflection and random bounce based on roughness
	float3 refl = reflect(WorldRayDirection(), normal_WS);
	float3 randomBounce = RandomCosineWeightedHemisphere(rand(rng), rand(rng.yx), normal_WS);
	float3 dir = normalize(lerp(refl, randomBounce, thisEntity.Color.a));
		
	// Create the new recursive ray
	RayDesc ray;
	ray.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	ray.Direction = dir;
	ray.TMin = 0.0001f;
	ray.TMax = 1000.0f;
	
	// Recursive ray trace
	payload.RecursionDepth++;
	
	RaytracingAccelerationStructure SceneTLAS = ResourceDescriptorHeap[SceneTLASDescriptorIndex];
	TraceRay(
		SceneTLAS,
		RAY_FLAG_NONE,
		0xFF, 0, 0, 0, // Mask and offsets
		ray,
		payload);
}