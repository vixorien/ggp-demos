#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include <unordered_map>

#include "SimpleShader.h"
#include "Camera.h"
#include "Transform.h"

class Material
{
public:
	Material(
		std::shared_ptr<SimplePixelShader> ps, 
		std::shared_ptr<SimpleVertexShader> vs, 
		DirectX::XMFLOAT3 tint, 
		DirectX::XMFLOAT2 uvScale = DirectX::XMFLOAT2(1, 1),
		DirectX::XMFLOAT2 uvOffset = DirectX::XMFLOAT2(0, 0),
		bool transparent = false);

	std::shared_ptr<SimplePixelShader> GetPixelShader();
	std::shared_ptr<SimpleVertexShader> GetVertexShader();
	DirectX::XMFLOAT2 GetUVScale();
	DirectX::XMFLOAT2 GetUVOffset();
	DirectX::XMFLOAT3 GetColorTint();
	bool GetTransparent();
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTextureSRV(std::string name);
	Microsoft::WRL::ComPtr<ID3D11SamplerState> GetSampler(std::string name);

	void SetPixelShader(std::shared_ptr<SimplePixelShader> ps);
	void SetVertexShader(std::shared_ptr<SimpleVertexShader> ps);
	void SetUVScale(DirectX::XMFLOAT2 scale);
	void SetUVOffset(DirectX::XMFLOAT2 offset);
	void SetColorTint(DirectX::XMFLOAT3 tint);
	void SetTransparent(bool transparent);

	void AddTextureSRV(std::string name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv);
	void AddSampler(std::string name, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler);

	void RemoveTextureSRV(std::string name);
	void RemoveSampler(std::string name);

	void PrepareMaterial(Transform* transform, Camera* camera);

private:

	// Shaders
	std::shared_ptr<SimplePixelShader> ps;
	std::shared_ptr<SimpleVertexShader> vs;
	
	// Material properties
	DirectX::XMFLOAT3 colorTint;
	bool transparent;

	// Texture-related
	DirectX::XMFLOAT2 uvOffset;
	DirectX::XMFLOAT2 uvScale;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textureSRVs;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11SamplerState>> samplers;
};
