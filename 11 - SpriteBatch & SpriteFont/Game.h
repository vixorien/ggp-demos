#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "Material.h"
#include "SimpleShader.h"
#include "Lights.h"
#include "Sky.h"

#include "SpriteBatch.h"
#include "SpriteFont.h"

#include <vector>
#include <DirectXMath.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects

class Game 
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	void Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);

private:

	// Initialization helper methods - feel free to customize, combine, etc.
	void LoadAssetsAndCreateEntities();

	// Camera for the 3D scene
	Camera* camera;

	// The sky box
	Sky* sky;
	bool skyEnabled;

	// A vector to hold any number of meshes
	// - This makes things easy to draw and clean up, too!
	std::vector<Mesh*> meshes;
	std::vector<Material*> materials;
	std::vector<GameEntity*> entities;
	std::vector<Light> lights;
	DirectX::XMFLOAT3 ambientColor;

	// Sprite batch resources
	std::shared_ptr<DirectX::SpriteBatch> spriteBatch;
	std::shared_ptr<DirectX::SpriteFont> fontArial12;
	std::shared_ptr<DirectX::SpriteFont> fontArial12Bold;
	std::shared_ptr<DirectX::SpriteFont> fontArial16;
	std::shared_ptr<DirectX::SpriteFont> fontArial16Bold;
};
