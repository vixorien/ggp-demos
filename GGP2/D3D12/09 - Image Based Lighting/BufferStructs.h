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

	unsigned int irradianceIndex;
	unsigned int specularIndex;
	unsigned int brdfLUTIndex;
	unsigned int totalSpecularMipLevels;

	Light lights[MAX_LIGHTS];

	DirectX::XMFLOAT4 SHIrradianceValues[9];
	unsigned int UseSH;
	
	unsigned int IndirectLightingEnabled;
};

struct PixelShaderPerObjectData
{
	unsigned int albedoIndex;
	unsigned int normalMapIndex;
	unsigned int roughnessIndex;
	unsigned int metalnessIndex;
	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT2 uvOffset;
	DirectX::XMFLOAT3 colorTint;
	float roughness;
	float metalness;
};

struct EnvPreviewData
{
	DirectX::XMFLOAT4 SHIrradianceValues[9];
	unsigned int UseSH;
	unsigned int SkyboxDescriptorIndex;
};