#pragma once

#include <DirectXMath.h>

// Overall scene data for ray tracing
struct RaytracingSceneData
{
	DirectX::XMFLOAT4X4 InverseViewProjection;
	DirectX::XMFLOAT3 CameraPosition;
	float pad;
};