#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include <unordered_map>
#include <string>

#include "Camera.h"
#include "Transform.h"

class Material
{
public:
	Material(
		const char* name,
		Microsoft::WRL::ComPtr<ID3D11PixelShader> ps,
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vs,
		DirectX::XMFLOAT3 tint,
		float roughness,
		bool useSpecularMap = false,
		DirectX::XMFLOAT2 uvScale = DirectX::XMFLOAT2(1, 1),
		DirectX::XMFLOAT2 uvOffset = DirectX::XMFLOAT2(0, 0));

	Microsoft::WRL::ComPtr<ID3D11PixelShader> GetPixelShader();
	Microsoft::WRL::ComPtr<ID3D11VertexShader> GetVertexShader();
	DirectX::XMFLOAT3 GetColorTint();
	float GetRoughness();
	bool GetUseSpecularMap();
	DirectX::XMFLOAT2 GetUVScale();
	DirectX::XMFLOAT2 GetUVOffset();
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTextureSRV(unsigned int index);
	Microsoft::WRL::ComPtr<ID3D11SamplerState> GetSampler(unsigned int index);
	const char* GetName();

	std::unordered_map<unsigned int, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>& GetTextureSRVMap();
	std::unordered_map<unsigned int, Microsoft::WRL::ComPtr<ID3D11SamplerState>>& GetSamplerMap();

	void SetPixelShader(Microsoft::WRL::ComPtr<ID3D11PixelShader> ps);
	void SetVertexShader(Microsoft::WRL::ComPtr<ID3D11VertexShader> ps);
	void SetColorTint(DirectX::XMFLOAT3 tint);
	void SetRoughness(float rough);
	void SetUseSpecularMap(bool spec);
	void SetUVScale(DirectX::XMFLOAT2 scale);
	void SetUVOffset(DirectX::XMFLOAT2 offset);

	void AddTextureSRV(unsigned int index, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv);
	void AddSampler(unsigned int index, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler);

	void RemoveTextureSRV(unsigned int index);
	void RemoveSampler(unsigned int index);

	void BindTexturesAndSamplers();

private:

	// Name (mostly for UI purposes)
	const char* name;

	// Shaders
	Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;

	// Material properties
	DirectX::XMFLOAT3 colorTint;
	float roughness;
	bool useSpecularMap;

	// Texture-related
	DirectX::XMFLOAT2 uvOffset;
	DirectX::XMFLOAT2 uvScale;
	std::unordered_map<unsigned int, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textureSRVs;
	std::unordered_map<unsigned int, Microsoft::WRL::ComPtr<ID3D11SamplerState>> samplers;
};

