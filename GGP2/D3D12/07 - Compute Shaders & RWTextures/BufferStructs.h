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

struct ComputeDescriptorIndices
{
	unsigned int noiseTextureIndex;
	unsigned int albedoTextureIndex;
	unsigned int normalTextureIndex;
	unsigned int roughTextureIndex;
	unsigned int metalTextureIndex;
	float time;
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