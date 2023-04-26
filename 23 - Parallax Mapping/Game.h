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

	// General helpers for setup and drawing
	void GenerateLights();
	void DrawLightSources();

	// Camera for the 3D scene
	std::shared_ptr<Camera> camera;

	// The sky box
	std::shared_ptr<Sky> sky;

	// Vectors for objects we'll need to clean up
	std::vector<std::shared_ptr<GameEntity>> entities;

	// Entity options
	bool pauseMovement;
	float movementTime;

	// Lights
	std::vector<Light> lights;
	DirectX::XMFLOAT3 ambientColor;
	int lightCount;
	bool freezeLightMovement;
	bool drawLights;

	// UI functions
	void UINewFrame(float deltaTime);
	void BuildUI();

	// Should the ImGui demo window be shown?
	bool showUIDemoWindow;
};

