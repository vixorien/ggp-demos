#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include "Mesh.h"
#include "Transform.h"
#include "Camera.h"

class GameEntity
{
public:
	GameEntity(Mesh* mesh);

	Mesh* GetMesh();
	void SetMesh(Mesh* mesh);

	Transform* GetTransform();

	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, Microsoft::WRL::ComPtr<ID3D11Buffer> vsConstantBuffer, Camera* camera);

private:

	Mesh* mesh;
	Transform transform;
};

