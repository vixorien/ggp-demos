#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>

#include "Camera.h"
#include "Transform.h"

class Material
{
public:
	Material(const char* name, Microsoft::WRL::ComPtr<ID3D11PixelShader> ps, Microsoft::WRL::ComPtr<ID3D11VertexShader> vs, DirectX::XMFLOAT3 tint);

	Microsoft::WRL::ComPtr<ID3D11PixelShader> GetPixelShader();
	Microsoft::WRL::ComPtr<ID3D11VertexShader> GetVertexShader();
	DirectX::XMFLOAT3 GetColorTint();
	const char* GetName();

	void SetPixelShader(Microsoft::WRL::ComPtr<ID3D11PixelShader> ps);
	void SetVertexShader(Microsoft::WRL::ComPtr<ID3D11VertexShader> ps);
	void SetColorTint(DirectX::XMFLOAT3 tint);

private:

	// Name (mostly for UI purposes)
	const char* name;

	// Shaders
	Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;

	// Material properties
	DirectX::XMFLOAT3 colorTint;
};

