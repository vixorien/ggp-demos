#pragma once

#include "Lights.h"
#include <DirectXMath.h>

struct DrawDescriptorIndices
{
	unsigned int msVertexBufferIndex;
	unsigned int msMeshletBufferIndex;
	unsigned int msVertexIndicesBufferIndex;
	unsigned int msTriangleIndicesBufferIndex;

	unsigned int msPerFrameCBIndex;
	unsigned int msPerObjectCBIndex;
	unsigned int psPerFrameCBIndex;
	unsigned int psPerObjectCBIndex;
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