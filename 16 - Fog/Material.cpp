#include "Material.h"

Material::Material(const char* name, std::shared_ptr<SimplePixelShader> ps, std::shared_ptr<SimpleVertexShader> vs, DirectX::XMFLOAT3 tint, float roughness) :
	name(name), 
	ps(ps),
	vs(vs),
	colorTint(tint),
	roughness(roughness)
{

}

std::shared_ptr<SimplePixelShader> Material::GetPixelShader() { return ps; }
std::shared_ptr<SimpleVertexShader> Material::GetVertexShader() { return vs; }
DirectX::XMFLOAT3 Material::GetColorTint() { return colorTint; }
float Material::GetRoughness() { return roughness; }
const char* Material::GetName() { return name; }

void Material::SetPixelShader(std::shared_ptr<SimplePixelShader> ps) { this->ps = ps; }
void Material::SetVertexShader(std::shared_ptr<SimpleVertexShader> vs) { this->vs = vs; }
void Material::SetColorTint(DirectX::XMFLOAT3 tint) { this->colorTint = tint; }
void Material::SetRoughness(float rough) { roughness = rough; }

void Material::PrepareMaterial(std::shared_ptr<Transform> transform, std::shared_ptr<Camera> camera)
{
	// Turn on these shaders
	vs->SetShader();
	ps->SetShader();

	// Send data to the vertex shader
	vs->SetMatrix4x4("world", transform->GetWorldMatrix());
	vs->SetMatrix4x4("worldInvTrans", transform->GetWorldInverseTransposeMatrix());
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());
	vs->CopyAllBufferData();

	// Send data to the pixel shader
	ps->SetFloat3("colorTint", colorTint);
	ps->SetFloat("roughness", roughness);
	ps->SetFloat3("cameraPosition", camera->GetTransform()->GetPosition());
	ps->CopyAllBufferData();
}
