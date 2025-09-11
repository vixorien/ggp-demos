#pragma once

#include <DirectXMath.h>

// This struct defines the data we want to
// send to the GPU.  This layout must match
// the corresponding cbuffer definition in
// our Vertex Shader or there will be a
// mismatch of data.
struct VertexShaderExternalData
{
	DirectX::XMFLOAT4 ColorTint;
	DirectX::XMFLOAT3 Offset;
};