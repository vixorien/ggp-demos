#include "GameEntity.h"
#include "BufferStructs.h"

#include "Graphics.h"

using namespace DirectX;

GameEntity::GameEntity(
	std::shared_ptr<Mesh> mesh, 
	std::shared_ptr<Material> material) 
	:
	mesh(mesh),
	material(material)
{
	transform = std::make_shared<Transform>();
}

// Getters
std::shared_ptr<Mesh> GameEntity::GetMesh() { return mesh; }
std::shared_ptr<Material> GameEntity::GetMaterial() { return material; }
std::shared_ptr<Transform> GameEntity::GetTransform() { return transform; }

// Setters
void GameEntity::SetMesh(std::shared_ptr<Mesh> mesh) { this->mesh = mesh; }
void GameEntity::SetMaterial(std::shared_ptr<Material> material) { this->material = material; }

