#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include <memory>
#include "Mesh.h"
#include "Transform.h"
#include "Camera.h"

class GameEntity
{
public:
	GameEntity(std::shared_ptr<Mesh> mesh);

	std::shared_ptr<Mesh> GetMesh();
	std::shared_ptr<Transform> GetTransform();
	
	void SetMesh(std::shared_ptr<Mesh> mesh);

private:

	std::shared_ptr<Mesh> mesh;
	std::shared_ptr<Transform> transform;
};

