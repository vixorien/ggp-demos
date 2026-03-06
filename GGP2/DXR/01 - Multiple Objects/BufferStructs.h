#pragma once

#include <DirectXMath.h>

// Root constants for bindless resources
struct RayTracingDrawData
{
	unsigned int SceneDataCBIndex;
	unsigned int EntityDataDescriptorIndex;
	unsigned int SceneTLASDescriptorIndex;
	unsigned int OutputUAVDescriptorIndex;
};

// Overall scene data for ray tracing (constant buffer)
struct RayTracingSceneData
{
	DirectX::XMFLOAT4X4 InverseViewProjection;
	DirectX::XMFLOAT3 CameraPosition;
	float pad;
};

// Per-entity information (geom, material, etc.)
// - Multiple sets of these will be stored in a structured buffer
// - Materials *could* be separated out into their own buffer 
//   to cut down on data repetition
struct RayTracingEntityData
{
	DirectX::XMFLOAT4 Color;
	unsigned int VertexBufferDescriptorIndex;
	unsigned int IndexBufferDescriptorIndex;
	float pad[2];
};