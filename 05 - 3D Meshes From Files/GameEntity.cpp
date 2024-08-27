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



void GameEntity::Draw(Microsoft::WRL::ComPtr<ID3D11Buffer> vsConstantBuffer, std::shared_ptr<Camera> camera)
{
	// Create a place to collect the vertex shader data locally
	//  - We need to do this because there is no built-in mechanism
	//     for directly accessing cbuffer variables in GPU memory
	//  - So, instead, we're filling up a struct that has the same
	//     layout as the cbuffer, so we can copy it in one step
	VertexShaderExternalData vsData = {};
	vsData.worldMatrix = transform->GetWorldMatrix();
	vsData.viewMatrix = camera->GetView();
	vsData.projectionMatrix = camera->GetProjection();

	// Copy this data to the constant buffer we intend to use
	D3D11_MAPPED_SUBRESOURCE mappedBuffer = {};
	Graphics::Context->Map(vsConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedBuffer);

	// Straight memcpy() into the resource
	memcpy(mappedBuffer.pData, &vsData, sizeof(vsData));

	// Unmap so the GPU can once again use the buffer
	Graphics::Context->Unmap(vsConstantBuffer.Get(), 0);

	// Draw the mesh
	mesh->SetBuffersAndDraw();
}
