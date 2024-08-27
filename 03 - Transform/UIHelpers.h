#pragma once

#include <vector>
#include <memory>

#include "Mesh.h"
#include "GameEntity.h"

// Informing IMGUI about the new frame
void UINewFrame(float deltaTime);

// Overall UI creation for the frame
void BuildUI(
	std::vector<std::shared_ptr<Mesh>>& meshes,
	std::vector<std::shared_ptr<GameEntity>>& entities);

// Helpers for individual scene elements
void UIMesh(std::shared_ptr<Mesh> mesh);
void UIEntity(std::shared_ptr<GameEntity> entity);