#pragma once

#include <DirectXMath.h>

// Must match the MAX_LIGHTS definition in your shaders
#define MAX_LIGHTS 128

#define LIGHT_TYPE_DIRECTIONAL	0
#define LIGHT_TYPE_POINT		1
#define LIGHT_TYPE_SPOT			2

// Defines a single light that can be sent to the GPU
// Note: This must match light struct in shaders
//       and must also be a multiple of 16 bytes!
struct Light
{
	int					Type;
	DirectX::XMFLOAT3	Direction;	// 16 bytes

	float				Range;
	DirectX::XMFLOAT3	Position;	// 32 bytes

	float				Intensity;
	DirectX::XMFLOAT3	Color;		// 48 bytes

	float				SpotFalloff;
	int					CastsShadows;
	DirectX::XMFLOAT2	Padding;	// 64 bytes
};

// A struct to hold lighting-related, allowing them to be
// passed to the UI helper functions easier.
struct DemoLightingOptions
{
	int LightCount;
	bool FreezeLightMovement;
	float LightMoveTime;
	bool FreezeEntityMovement;
	float EntityMoveTime;
	bool DrawLights;
	DirectX::XMFLOAT3 AmbientColor;
};

// A struct to hold all shadow-related options, allowing them
// to be passed to the UI helper easier.
struct DemoShadowOptions
{
	int ShadowMapResolution;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> ShadowDSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShadowSRV;
	
	float ShadowProjectionSize;
	DirectX::XMFLOAT4X4 ShadowViewMatrix;
	DirectX::XMFLOAT4X4 ShadowProjectionMatrix;
};