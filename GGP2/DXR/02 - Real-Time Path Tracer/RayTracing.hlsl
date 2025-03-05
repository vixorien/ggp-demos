
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

// 11 floats total per vertex * 4 bytes each
static const uint VertexSizeInBytes = 11 * 4; 


// Payload for rays (data that is "sent along" with each ray during raytrace)
// Note: This should be as small as possible, and must match our C++ size definition
struct RayPayload
{
	float3 color;
	uint recursionDepth;
	uint rayPerPixelIndex;
};

// Note: We'll be using the built-in BuiltInTriangleIntersectionAttributes struct
// for triangle attributes, so no need to define our own.  It contains a single float2.



// === Constant buffers ===

cbuffer SceneData : register(b0)
{
	matrix inverseViewProjection;
	float3 cameraPosition;
	int raysPerPixel;
};

// Ensure this matches C++ buffer struct define!
#define MAX_INSTANCES_PER_BLAS 100
cbuffer ObjectData : register(b1)
{
	float4 entityColor[MAX_INSTANCES_PER_BLAS];
};


// === Resources ===

// Output UAV 
RWTexture2D<float4> OutputColor				: register(u0);

// The actual scene we want to trace through (a TLAS)
RaytracingAccelerationStructure SceneTLAS	: register(t0);

// Geometry buffers
ByteAddressBuffer IndexBuffer        		: register(t1);
ByteAddressBuffer VertexBuffer				: register(t2);


// === Helpers ===

// Loads the indices of the specified triangle from the index buffer
uint3 LoadIndices(uint triangleIndex)
{
	// What is the start index of this triangle's indices?
	uint indicesStart = triangleIndex * 3;

	// Adjust by the byte size before loading
	return IndexBuffer.Load3(indicesStart * 4); // 4 bytes per index
}


// Barycentric interpolation of data from the triangle's vertices
Vertex InterpolateVertices(uint triangleIndex, float2 barycentrics)
{
	// Calculate the barycentric data for vertex interpolation
	float3 barycentricData = float3(
		1.0f - barycentrics.x - barycentrics.y,
		barycentrics.x,
		barycentrics.y);

	// Grab the indices
	uint3 indices = LoadIndices(triangleIndex);

	// Set up the final vertex
    Vertex vert = (Vertex)0;

	// Loop through the barycentric data and interpolate
	for (uint i = 0; i < 3; i++)
	{
		// Get the index of the first piece of data for this vertex
		uint dataIndex = indices[i] * VertexSizeInBytes;

		// Grab the position and offset
		vert.localPosition += asfloat(VertexBuffer.Load3(dataIndex)) * barycentricData[i];
		dataIndex += 3 * 4; // 3 floats * 4 bytes per float

		// UV
		vert.uv += asfloat(VertexBuffer.Load2(dataIndex)) * barycentricData[i];
		dataIndex += 2 * 4; // 2 floats * 4 bytes per float

		// Normal
		vert.normal += asfloat(VertexBuffer.Load3(dataIndex)) * barycentricData[i];
		dataIndex += 3 * 4; // 3 floats * 4 bytes per float

		// Tangent (no offset at the end, since we start over after looping)
		vert.tangent += asfloat(VertexBuffer.Load3(dataIndex)) * barycentricData[i];
	}

	// Final interpolated vertex data is ready
	return vert;
}


// Calculates an origin and direction from the camera for specific pixel indices
RayDesc CalcRayFromCamera(float2 rayIndices)
{
	// Offset to the middle of the pixel
	float2 pixel = rayIndices + 0.5f;
	float2 screenPos = pixel / DispatchRaysDimensions().xy * 2.0f - 1.0f;
	screenPos.y = -screenPos.y;

	// Unproject the coords
	float4 worldPos = mul(inverseViewProjection, float4(screenPos, 0, 1));
	worldPos.xyz /= worldPos.w;

	// Set up the ray
	RayDesc ray;
	ray.Origin = cameraPosition.xyz;
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
	// Get the ray indices
	uint2 rayIndices = DispatchRaysIndex().xy;
	
	float rng = rand((float2)rayIndices / DispatchRaysDimensions().xy);
	
	OutputColor[rayIndices] = rng.xxxx;
	return;
	

	// Average of all rays per pixel
	float3 totalColor = float3(0, 0, 0);
	
	for (int r = 0; r < raysPerPixel; r++)
	{
		float2 adjustedIndices = (float2)rayIndices;
		float ray01 = (float)r / raysPerPixel;
		adjustedIndices += rand2(rayIndices.xy * ray01);
		
		// Calculate the ray from the camera through a particular
		// pixel of the output buffer using this shader's indices
		RayDesc ray = CalcRayFromCamera(adjustedIndices);

		// Set up the payload for the ray
		RayPayload payload;
		payload.color = float3(1, 1, 1);
		payload.recursionDepth = 0;
		payload.rayPerPixelIndex = r;
		
		// Perform the ray trace for this ray
		TraceRay(
			SceneTLAS,
			RAY_FLAG_NONE,
			0xFF,
			0,
			0,
			0,
			ray,
			payload);
		
		totalColor += payload.color;
	}
	
	// Set the final color of the buffer (gamma corrected)
	OutputColor[rayIndices] = float4(pow(totalColor / raysPerPixel, 1.0f / 2.2f), 1);
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
	payload.color *= skyColor;
}


// Closest hit shader - Runs the first time a ray hits anything
[shader("closesthit")]
void ClosestHit(inout RayPayload payload, BuiltInTriangleIntersectionAttributes hitAttributes)
{
	// If we've reached the max recursion, we haven't hit a light source (the sky, which is the "miss shader" here)
	if (payload.recursionDepth == 10)
	{
		payload.color = float3(0, 0, 0);
		return;
	}
	
	// We've hit, so adjust the payload color by this instance's color
	payload.color *= entityColor[InstanceID()].rgb;
	
	// Get the geometry hit details and convert normal to world space
	Vertex hit = InterpolateVertices(PrimitiveIndex(), hitAttributes.barycentrics);
	float3 normal_WS = normalize(mul(hit.normal, (float3x3)ObjectToWorld4x3()));
	
	// Calc a unique RNG value for this ray, based on the "uv" (0-1 location) of this pixel and other per-ray data
	float2 pixelUV = (float2)DispatchRaysIndex().xy / DispatchRaysDimensions().xy;
	float2 rng = rand2(pixelUV * (payload.recursionDepth + 1) + payload.rayPerPixelIndex + RayTCurrent());
	
	// Interpolate between perfect reflection and random bounce based on roughness
	float3 refl = reflect(WorldRayDirection(), normal_WS);
	float3 randomBounce = RandomCosineWeightedHemisphere(rand(rng), rand(rng.yx), normal_WS);
	float3 dir = normalize(lerp(refl, randomBounce, entityColor[InstanceID()].a));
	
	// Create the new recursive ray
	RayDesc ray;
	ray.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
	ray.Direction = dir;
	ray.TMin = 0.0001f;
	ray.TMax = 1000.0f;
	
	// Recursive ray trace
	payload.recursionDepth++;
	TraceRay(
		SceneTLAS,
		RAY_FLAG_NONE,
		0xFF, 0, 0, 0, // Mask and offsets
		ray,
		payload);
}