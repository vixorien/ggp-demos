#include "Material.h"

Material::Material(SimplePixelShader* ps, SimpleVertexShader* vs, DirectX::XMFLOAT3 tint) :
	ps(ps),
	vs(vs),
	colorTint(tint)
{

}

SimplePixelShader* Material::GetPixelShader() { return ps; }
SimpleVertexShader* Material::GetVertexShader() { return vs; }
DirectX::XMFLOAT3 Material::GetColorTint() { return colorTint; }

void Material::SetPixelShader(SimplePixelShader* ps) { this->ps = ps; }
void Material::SetVertexShader(SimpleVertexShader* vs) { this->vs = vs; }
void Material::SetColorTint(DirectX::XMFLOAT3 tint) { this->colorTint = tint; }

void Material::PrepareMaterial(Transform* transform, Camera* camera)
{
	// Turn on these shaders
	vs->SetShader();
	ps->SetShader();

	// Send data to the vertex shader
	vs->SetMatrix4x4("world", transform->GetWorldMatrix());
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());
	vs->CopyAllBufferData();

	// Send data to the pixel shader
	ps->SetFloat3("colorTint", colorTint);
	ps->CopyAllBufferData();
}
