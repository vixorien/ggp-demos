#pragma once

#include "Lights.h"
#include <DirectXMath.h>

// Must match vertex shader definition!
struct VertexShaderExternalData
{
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 worldInverseTranspose;
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};

// Must match pixel shader definition!
struct PixelShaderExternalData
{
	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT2 uvOffset;
	DirectX::XMFLOAT3 cameraPosition;
	int lightCount;
	Light lights[MAX_LIGHTS];
};

// Overall scene data for raytracing
struct RaytracingSceneData
{
	DirectX::XMFLOAT4X4 inverseViewProjection;
	DirectX::XMFLOAT3 cameraPosition;
	int raysPerPixel;
};

// All material data for raytracing
struct RaytracingMaterial
{
	// 16 bytes
	DirectX::XMFLOAT3 color;
	float roughness;

	// 16 bytes
	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT2 uvOffset;

	// 16 bytes
	float metal;
	DirectX::XMFLOAT3 padding;

	// 16 bytes
	unsigned int albedoIndex;
	unsigned int normalMapIndex;
	unsigned int roughnessIndex;
	unsigned int metalnessIndex;
};

// Ensure this matches Raytracing shader define!
#define MAX_INSTANCES_PER_BLAS 100
struct RaytracingEntityData
{
	RaytracingMaterial materials[MAX_INSTANCES_PER_BLAS];
};