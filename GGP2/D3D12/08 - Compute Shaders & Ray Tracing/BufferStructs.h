#pragma once

#include "Lights.h"
#include <DirectXMath.h>



struct DrawDescriptorIndices
{
	unsigned int vsVertexBufferIndex;
	unsigned int vsPerFrameCBIndex;
	unsigned int vsPerObjectCBIndex;
	unsigned int psPerFrameCBIndex;
	unsigned int psPerObjectCBIndex;
};

#define MAX_SPHERES 32

struct Sphere
{
	DirectX::XMFLOAT3 Position;
	float Radius;
	DirectX::XMFLOAT3 Color;
	float Roughness;
};

struct DrawData
{
	Sphere spheres[MAX_SPHERES];
	DirectX::XMFLOAT4X4 invVP;
	DirectX::XMFLOAT3 cameraPosition;
	unsigned int sphereCount;
	DirectX::XMFLOAT3 skyColor;
	unsigned int windowWidth;
	unsigned int windowHeight;
	unsigned int maxRecursion;
	unsigned int raysPerPixel;
};

struct ComputeDescriptorIndices
{
	unsigned int cbIndex;
	unsigned int outputTextureIndex;
};

// Must match vertex shader definition!
struct VertexShaderPerFrameData
{
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};

struct VertexShaderPerObjectData
{
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 worldInverseTranspose;
};

// Must match pixel shader definition!
struct PixelShaderPerFrameData
{
	DirectX::XMFLOAT3 cameraPosition;
	int lightCount;
	Light lights[MAX_LIGHTS];
};

struct PixelShaderPerObjectData
{
	unsigned int albedoIndex;
	unsigned int normalMapIndex;
	unsigned int roughnessIndex;
	unsigned int metalnessIndex;
	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT2 uvOffset;
};