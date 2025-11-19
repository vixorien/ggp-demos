#pragma once

#include "Lights.h"

#include <DirectXMath.h>

struct VertexShaderExternalData
{
	DirectX::XMFLOAT4X4 worldMatrix;
	DirectX::XMFLOAT4X4 worldInvTransMatrix;
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;

	DirectX::XMFLOAT4X4 lightViewMatrix;
	DirectX::XMFLOAT4X4 lightProjMatrix;
};

struct PixelShaderExternalData
{
	// Scene related
	Light lights[MAX_LIGHTS];

	int lightCount;
	DirectX::XMFLOAT3 ambientColor;

	// Camera related
	DirectX::XMFLOAT3 cameraPosition;
	float pad; // Alignment

	// Material related
	DirectX::XMFLOAT3 colorTint;
	float pad2;

	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT2 uvOffset;
};