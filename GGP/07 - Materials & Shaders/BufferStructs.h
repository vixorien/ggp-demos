#pragma once

#include <DirectXMath.h>

struct VertexShaderExternalData
{
	DirectX::XMFLOAT4X4 worldMatrix;
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;
};

struct PixelShaderExternalData
{
	DirectX::XMFLOAT3 colorTint;
	float time;
};