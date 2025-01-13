#pragma once

#include <vector>
#include <memory>

#include "Mesh.h"

// Informing IMGUI about the new frame
void UINewFrame(float deltaTime);

// Overall UI creation for the frame
void BuildUI(std::vector<std::shared_ptr<Mesh>>& meshes);

// Helpers for individual scene elements
void UIMesh(std::shared_ptr<Mesh> mesh);