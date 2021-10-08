#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include "Mesh.h"
#include "Transform.h"

class GameEntity
{
public:
	GameEntity(Mesh* mesh);

	Mesh* GetMesh();
	Transform* GetTransform();

	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, Microsoft::WRL::ComPtr<ID3D11Buffer> vsConstantBuffer);

private:

	Mesh* mesh;
	Transform transform;
};

