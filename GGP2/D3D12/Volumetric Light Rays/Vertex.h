#pragma once

#include <DirectXMath.h>

struct Vertex
{
	DirectX::XMFLOAT3 Position;	    // The local position of the vertex
	DirectX::XMFLOAT2 UV;			// The color of the vertex
	DirectX::XMFLOAT3 Normal;		// Normal for lighting
	DirectX::XMFLOAT3 Tangent;		// Tangent for normal mapping
};