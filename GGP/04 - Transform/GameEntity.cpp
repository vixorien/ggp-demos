#include "GameEntity.h"
#include "BufferStructs.h"

#include "Graphics.h"

using namespace DirectX;

GameEntity::GameEntity(std::shared_ptr<Mesh> mesh)
	: mesh(mesh)
{
	transform = std::make_shared<Transform>();
}

// Getters
std::shared_ptr<Mesh> GameEntity::GetMesh() { return mesh; }
std::shared_ptr<Transform> GameEntity::GetTransform() { return transform; }

// Setters
void GameEntity::SetMesh(std::shared_ptr<Mesh> mesh) { this->mesh = mesh; }


void GameEntity::Draw()
{
	// Draw the mesh
	mesh->SetBuffersAndDraw();
}
