#pragma once

#include <vector>
#include <memory>
#include <DirectXMath.h>

#include "Camera.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Material.h"
#include "Lights.h"


struct DemoOptions
{
	int ReflectionCount;
	float AmbientAmount;
	bool Shadows;
	float ShadowSpread;
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
	DemoOptions& demoOptions);

// Helpers for individual scene elements
void UIMesh(std::shared_ptr<Mesh> mesh);
void UIEntity(std::shared_ptr<GameEntity> entity);
void UICamera(std::shared_ptr<Camera> cam);
void UIMaterial(std::shared_ptr<Material> material);
void UILight(Light& light);
