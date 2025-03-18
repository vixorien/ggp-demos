#pragma once

#include "Mesh.h"
#include "GameEntity.h"
#include "Transform.h"
#include "Camera.h"
#include "Lights.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

class Game
{
public:
	// Basic OOP setup
	Game() = default;
	~Game();
	Game(const Game&) = delete; // Remove copy constructor
	Game& operator=(const Game&) = delete; // Remove copy-assignment operator

	// Primary functions
	void Initialize();
	void Update(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);
	void OnResize();

private:

	// UI functions and variables
	void UINewFrame(float deltaTime);
	void BuildUI();
	bool showUIDemoWindow;

	// Scene
	std::shared_ptr<FPSCamera> camera;
	std::vector<std::shared_ptr<GameEntity>> entities;
	D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle;
};

