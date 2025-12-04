#include "Material.h"
#include "Graphics.h"

Material::Material(
	const char* name, 
	Microsoft::WRL::ComPtr<ID3D11PixelShader> ps,
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vs,
	DirectX::XMFLOAT3 tint,
	DirectX::XMFLOAT2 uvScale,
	DirectX::XMFLOAT2 uvOffset,
	bool transparent,
	float alphaClipThreshold)
	:
	name(name),
	ps(ps),
	vs(vs),
	colorTint(tint),
	uvScale(uvScale),
	uvOffset(uvOffset),
	transparent(transparent),
	alphaClipThreshold(alphaClipThreshold)
{

}

Microsoft::WRL::ComPtr<ID3D11PixelShader> Material::GetPixelShader() { return ps; }
Microsoft::WRL::ComPtr<ID3D11VertexShader> Material::GetVertexShader() { return vs; }
DirectX::XMFLOAT3 Material::GetColorTint() { return colorTint; }
DirectX::XMFLOAT2 Material::GetUVScale() { return uvScale; }
DirectX::XMFLOAT2 Material::GetUVOffset() { return uvOffset; }
const char* Material::GetName() { return name; }
bool Material::GetTransparent() { return transparent; }
float Material::GetAlphaClipThreshold() { return alphaClipThreshold; }

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Material::GetTextureSRV(unsigned int index)
{
	// Search for the key
	auto it = textureSRVs.find(index);

	// Not found, return null
	if (it == textureSRVs.end())
		return 0;

	// Return the texture ComPtr
	return it->second;
}

Microsoft::WRL::ComPtr<ID3D11SamplerState> Material::GetSampler(unsigned int index)
{
	// Search for the key
	auto it = samplers.find(index);

	// Not found, return null
	if (it == samplers.end())
		return 0;

	// Return the sampler ComPtr
	return it->second;
}

std::unordered_map<unsigned int, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>& Material::GetTextureSRVMap()
{
	return textureSRVs;
}

std::unordered_map<unsigned int, Microsoft::WRL::ComPtr<ID3D11SamplerState>>& Material::GetSamplerMap()
{
	return samplers;
}

void Material::SetPixelShader(Microsoft::WRL::ComPtr<ID3D11PixelShader> ps) { this->ps = ps; }
void Material::SetVertexShader(Microsoft::WRL::ComPtr<ID3D11VertexShader> vs) { this->vs = vs; }
void Material::SetColorTint(DirectX::XMFLOAT3 tint) { this->colorTint = tint; }
void Material::SetUVScale(DirectX::XMFLOAT2 scale) { uvScale = scale; }
void Material::SetUVOffset(DirectX::XMFLOAT2 offset) { uvOffset = offset; }
void Material::SetTransparent(bool transparent) { this->transparent = transparent; }
void Material::SetAlphaClipThreshold(float clipThreshold) { this->alphaClipThreshold = clipThreshold; }

void Material::AddTextureSRV(unsigned int index, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv)
{
	textureSRVs.insert({ index, srv });
}

void Material::AddSampler(unsigned int index, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler)
{
	samplers.insert({ index, sampler });
}

void Material::RemoveTextureSRV(unsigned int index)
{
	textureSRVs.erase(index);
}

void Material::RemoveSampler(unsigned int index)
{
	samplers.erase(index);
}

void Material::BindTexturesAndSamplers()
{
	for (auto& t : textureSRVs) { Graphics::Context->PSSetShaderResources(t.first, 1, t.second.GetAddressOf()); }
	for (auto& s : samplers) { Graphics::Context->PSSetSamplers(s.first, 1, s.second.GetAddressOf()); }
}
