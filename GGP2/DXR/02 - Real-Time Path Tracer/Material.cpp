#include "Material.h"
#include "Graphics.h"

Material::Material(
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState,
	DirectX::XMFLOAT3 tint,
	DirectX::XMFLOAT2 uvScale,
	DirectX::XMFLOAT2 uvOffset)
	:
	pipelineState(pipelineState),
	colorTint(tint),
	uvScale(uvScale),
	uvOffset(uvOffset),
	albedoIndex(-1),
	normalMapIndex(-1),
	roughnessIndex(-1),
	metalnessIndex(-1)
{
}

// Getters
Microsoft::WRL::ComPtr<ID3D12PipelineState> Material::GetPipelineState() { return pipelineState; }
DirectX::XMFLOAT2 Material::GetUVScale() { return uvScale; }
DirectX::XMFLOAT2 Material::GetUVOffset() { return uvOffset; }
DirectX::XMFLOAT3 Material::GetColorTint() { return colorTint; }

unsigned int Material::GetAlbedoIndex() { return albedoIndex; }
unsigned int Material::GetNormalMapIndex() { return normalMapIndex; }
unsigned int Material::GetRoughnessIndex() { return roughnessIndex; }
unsigned int Material::GetMetalnessIndex() { return metalnessIndex; }


// Setters
void Material::SetPipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState) { this->pipelineState = pipelineState; }
void Material::SetUVScale(DirectX::XMFLOAT2 scale) { uvScale = scale; }
void Material::SetUVOffset(DirectX::XMFLOAT2 offset) { uvOffset = offset; }
void Material::SetColorTint(DirectX::XMFLOAT3 tint) { this->colorTint = tint; }

void Material::SetAlbedoIndex(unsigned int index) { albedoIndex = index; }
void Material::SetNormalMapIndex(unsigned int index) { normalMapIndex = index; }
void Material::SetRoughnessIndex(unsigned int index) { roughnessIndex = index; }
void Material::SetMetalnessIndex(unsigned int index) { metalnessIndex = index; }
