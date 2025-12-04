#pragma once

#include "Lights.h"

#include <DirectXMath.h>

struct VertexShaderExternalData
{
	DirectX::XMFLOAT4X4 worldMatrix;
	DirectX::XMFLOAT4X4 worldInvTransMatrix;
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;
};

struct PixelShaderExternalData
{
	// Scene related
	Light lights[MAX_LIGHTS];

	int lightCount;
	DirectX::XMFLOAT3 ambientColor;

	// Camera related
	DirectX::XMFLOAT3 cameraPosition;
	float farClipDistance; // Necessary for fog

	// Material and fog
	DirectX::XMFLOAT3 colorTint;
	int fogType;

	DirectX::XMFLOAT3 fogColor;
	float fogStartDist;

	float fogEndDist;
	float fogDensity;
	int heightBasedFog;
	float fogVerticalDensity;

	float fogHeight;
};