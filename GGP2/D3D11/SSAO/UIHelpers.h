#pragma once

#include <vector>
#include <memory>
#include <DirectXMath.h>

#include "Camera.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Material.h"
#include "Lights.h"
#include "Sky.h"

#include "ImGui/imgui.h"

struct SSAOOptions
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ColorDirectSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ColorAmbientSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> NormalsSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> DepthSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SSAOResultsSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SSAOBlurSRV;

	bool SSAOEnabled;
	bool SSAOOutputOnly;
	int SampleCount;
	float SampleRadius;
};

// Informing IMGUI about the new frame
void UINewFrame(float deltaTime);

// Overall UI creation for the frame
void BuildUI(
	std::shared_ptr<Camera> camera,
	std::vector<std::shared_ptr<Mesh>>& meshes,
	std::vector<std::shared_ptr<GameEntity>>& entities,
	std::vector<std::shared_ptr<Material>>& materials,
	std::vector<Light>& lights,
	DemoLightingOptions& lightOptions,
	IBLOptions& iblOptions,
	SSAOOptions& ssaoOptions);

// Helpers for individual scene elements
void UIMesh(std::shared_ptr<Mesh> mesh);
void UIEntity(std::shared_ptr<GameEntity> entity);
void UICamera(std::shared_ptr<Camera> cam);
void UIMaterial(std::shared_ptr<Material> material);
void UILight(Light& light);

void ImageWithHover(ImTextureID user_texture_id, const ImVec2& size);
