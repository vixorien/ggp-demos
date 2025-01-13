#pragma once

#include <vector>
#include <memory>
#include <DirectXMath.h>

#include "Camera.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Material.h"
#include "Lights.h"

enum ToonShadingType
{
	ToonShadingNone,
	ToonShadingRamp,
	ToonShadingConditionals
};

enum OutlineType
{
	OutlineNone,
	OutlineInsideOut,
	OutlineSobelFilter,
	OutlineSilhouette,
	OutlineDepthNormals
};

struct ToonOptions
{
	int LightCount;
	bool DrawLights;
	bool FreezeLightMovement;
	bool FreezeEntityRotation;
	bool ShowRampTextures;
	bool ShowSpecularRamp;
	ToonShadingType ToonShadingMode;
	OutlineType OutlineMode;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SceneDepthsSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SceneNormalsSRV;
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
	ToonOptions& lightOptions);

// Helpers for individual scene elements
void UIMesh(std::shared_ptr<Mesh> mesh);
void UIEntity(std::shared_ptr<GameEntity> entity);
void UICamera(std::shared_ptr<Camera> cam);
void UIMaterial(std::shared_ptr<Material> material);
void UILight(Light& light);
