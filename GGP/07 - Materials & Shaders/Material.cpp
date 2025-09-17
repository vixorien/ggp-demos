#include "Material.h"

Material::Material(const char* name, Microsoft::WRL::ComPtr<ID3D11PixelShader> ps, Microsoft::WRL::ComPtr<ID3D11VertexShader> vs, DirectX::XMFLOAT3 tint) :
	name(name),
	ps(ps),
	vs(vs),
	colorTint(tint)
{

}

Microsoft::WRL::ComPtr<ID3D11PixelShader> Material::GetPixelShader() { return ps; }
Microsoft::WRL::ComPtr<ID3D11VertexShader> Material::GetVertexShader() { return vs; }
DirectX::XMFLOAT3 Material::GetColorTint() { return colorTint; }
const char* Material::GetName() { return name; }

void Material::SetPixelShader(Microsoft::WRL::ComPtr<ID3D11PixelShader> ps) { this->ps = ps; }
void Material::SetVertexShader(Microsoft::WRL::ComPtr<ID3D11VertexShader> vs) { this->vs = vs; }
void Material::SetColorTint(DirectX::XMFLOAT3 tint) { this->colorTint = tint; }

