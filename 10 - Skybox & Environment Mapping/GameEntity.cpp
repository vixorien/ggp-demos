#include "GameEntity.h"

using namespace DirectX;

GameEntity::GameEntity(Mesh* mesh, Material* material) :
	mesh(mesh),
	material(material)
{
}

Mesh* GameEntity::GetMesh() { return mesh; }
Material* GameEntity::GetMaterial() { return material; }
Transform* GameEntity::GetTransform() { return &transform; }

void GameEntity::SetMesh(Mesh* mesh) { this->mesh = mesh; }
void GameEntity::SetMaterial(Material* material) { this->material = material; }


void GameEntity::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, Camera* camera)
{
	// Set up the material (shaders)
	material->PrepareMaterial(&transform, camera);

	// Draw the mesh
	mesh->SetBuffersAndDraw(context);
}
