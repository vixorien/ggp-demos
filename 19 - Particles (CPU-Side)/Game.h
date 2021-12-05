#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "Material.h"
#include "SimpleShader.h"
#include "Lights.h"
#include "Sky.h"
#include "Emitter.h"

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

	// General helpers for setup and drawing
	void GenerateLights();
	void DrawLightSources();
	void DrawUI();

	// Camera for the 3D scene
	Camera* camera;

	// The sky box
	Sky* sky;

	// Vectors for objects we'll need to clean up
	std::vector<GameEntity*> entities;

	// Lights
	std::vector<Light> lights;
	DirectX::XMFLOAT3 ambientColor;
	int lightCount;

	// Sprite batch resources
	std::shared_ptr<DirectX::SpriteBatch> spriteBatch;

	// Particles
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> particleDepthState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> particleBlendState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> particleDebugRasterState;
	std::vector<Emitter*> emitters;
	void DrawParticles();
};

