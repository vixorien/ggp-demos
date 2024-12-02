#pragma once

#include <vector>
#include <memory>
#include <DirectXMath.h>

#include "Camera.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Material.h"
#include "Lights.h"

const int MaxDemoBloomLevels = 5;

struct DemoBloomOptions
{
	int CurrentBloomLevels;
	bool ShowBloomTextures;
	float BloomThreshold;
	float BloomLevelIntensities[MaxDemoBloomLevels];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> PostProcessSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> BloomExtractSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> BlurHorizontalSRVs[MaxDemoBloomLevels];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> BlurVerticalSRVs[MaxDemoBloomLevels];
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
	DemoBloomOptions& bloomOptions);

// Helpers for individual scene elements
void UIMesh(std::shared_ptr<Mesh> mesh);
void UIEntity(std::shared_ptr<GameEntity> entity);
void UICamera(std::shared_ptr<Camera> cam);
void UIMaterial(std::shared_ptr<Material> material);
void UILight(Light& light);
