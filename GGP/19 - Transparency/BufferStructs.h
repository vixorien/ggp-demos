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

	DirectX::XMFLOAT3 cameraPosition;
	int flipNormal;

	DirectX::XMFLOAT3 colorTint;
	float alphaClipThreshold;

	DirectX::XMFLOAT2 uvScale;
	DirectX::XMFLOAT2 uvOffset;

	int useNoiseForAlphaClip;
	float fadeDistStart;
	float fadeDistEnd;
};