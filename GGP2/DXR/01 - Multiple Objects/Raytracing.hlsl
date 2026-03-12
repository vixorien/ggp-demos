

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
	float pad;
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
	float3 color;
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
	StructuredBuffer<uint> IndexBuffer = ResourceDescriptorHeap[thisEntity.IndexBufferDescriptorIndex];
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

	// Calculate the ray from the camera through a particular
	// pixel of the output buffer using this shader's indices
	RayDesc ray = CalcRayFromCamera(
		rayIndices,
		cb.CameraPosition,
		cb.InverseViewProjection);

	// Set up the payload for the ray
	// This initializes the struct to all zeros
	RayPayload payload = (RayPayload)0;

	// Perform the ray trace for this ray
	RaytracingAccelerationStructure SceneTLAS = ResourceDescriptorHeap[SceneTLASDescriptorIndex];
	TraceRay(
		SceneTLAS,
		RAY_FLAG_NONE,
		0xFF,
		0,
		0,
		0,
		ray,
		payload);

	// Set the final color of the buffer (gamma corrected)
	RWTexture2D<float4> OutputColor = ResourceDescriptorHeap[OutputUAVDescriptorIndex];
	OutputColor[rayIndices] = float4(pow(payload.color, 1.0f / 2.2f), 1);
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
	payload.color = lerp(downColor, upColor, interpolation);
}


// Closest hit shader - Runs the first time a ray hits anything
[shader("closesthit")]
void ClosestHit(inout RayPayload payload, BuiltInTriangleIntersectionAttributes hitAttributes)
{
	// Not specifically using any triangle data here, but for reference:
	//Vertex interpolatedVert = InterpolateVertices(
	//	PrimitiveIndex(), 
	//	hitAttributes.barycentrics);
	
	// Get the data for this entity
	StructuredBuffer<EntityData> entityDataBuffer = ResourceDescriptorHeap[EntityDataDescriptorIndex];
	EntityData thisEntity = entityDataBuffer[InstanceIndex()];
	payload.color = thisEntity.Color.rgb;
}