#include "Material.h"
#include "Graphics.h"

Material::Material(
	const char* name,
	Microsoft::WRL::ComPtr<ID3D11PixelShader> ps,
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vs,
	DirectX::XMFLOAT3 tint,
	float roughness,
	bool useSpecularMap,
	DirectX::XMFLOAT2 uvScale,
	DirectX::XMFLOAT2 uvOffset)
	:
	name(name),
	ps(ps),
	vs(vs),
	colorTint(tint),
	roughness(roughness),
	useSpecularMap(useSpecularMap),
	uvScale(uvScale),
	uvOffset(uvOffset)
{

}

Microsoft::WRL::ComPtr<ID3D11PixelShader> Material::GetPixelShader() { return ps; }
Microsoft::WRL::ComPtr<ID3D11VertexShader> Material::GetVertexShader() { return vs; }
DirectX::XMFLOAT3 Material::GetColorTint() { return colorTint; }
float Material::GetRoughness() { return roughness; }
bool Material::GetUseSpecularMap() { return useSpecularMap; }
DirectX::XMFLOAT2 Material::GetUVScale() { return uvScale; }
DirectX::XMFLOAT2 Material::GetUVOffset() { return uvOffset; }
const char* Material::GetName() { return name; }

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
void Material::SetRoughness(float rough) { roughness = rough; }
void Material::SetUseSpecularMap(bool spec) { useSpecularMap = spec; }
void Material::SetUVScale(DirectX::XMFLOAT2 scale) { uvScale = scale; }
void Material::SetUVOffset(DirectX::XMFLOAT2 offset) { uvOffset = offset; }

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

//void Material::PrepareMaterial(std::shared_ptr<Transform> transform, std::shared_ptr<Camera> camera)
//{
//	// Turn on these shaders
//	vs->SetShader();
//	ps->SetShader();
//
//	// Send data to the vertex shader
//	vs->SetMatrix4x4("world", transform->GetWorldMatrix());
//	vs->SetMatrix4x4("worldInvTrans", transform->GetWorldInverseTransposeMatrix());
//	vs->SetMatrix4x4("view", camera->GetView());
//	vs->SetMatrix4x4("projection", camera->GetProjection());
//	vs->CopyAllBufferData();
//
//	// Send data to the pixel shader
//	ps->SetFloat3("colorTint", colorTint);
//	ps->SetFloat("roughness", roughness);
//	ps->SetInt("useSpecularMap", (int)useSpecularMap);
//	ps->SetFloat2("uvScale", uvScale);
//	ps->SetFloat2("uvOffset", uvOffset);
//	ps->SetFloat3("cameraPosition", camera->GetTransform()->GetPosition());
//	ps->CopyAllBufferData();
//
//	// Loop and set any other resources
//	for (auto& t : textureSRVs) { ps->SetShaderResourceView(t.first.c_str(), t.second.Get()); }
//	for (auto& s : samplers) { ps->SetSamplerState(s.first.c_str(), s.second.Get()); }
//}
