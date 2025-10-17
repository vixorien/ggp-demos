#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "Material.h"
#include "Lights.h"

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

	// Initialization helper methods - feel free to customize, combine, remove, etc.
	Microsoft::WRL::ComPtr<ID3D11PixelShader> LoadPixelShader(const wchar_t* compiledShaderPath);
	Microsoft::WRL::ComPtr<ID3D11VertexShader> LoadVertexShader(const wchar_t* compiledShaderPath);
	void LoadAssetsAndCreateEntities();

	// Camera for the 3D scene
	std::shared_ptr<FPSCamera> camera;

	// Scene data
	std::vector<std::shared_ptr<Mesh>> meshes;
	std::vector<std::shared_ptr<Material>> materials;
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::vector<Light> lights;
	DirectX::XMFLOAT3 ambientColor;

	// D3D API objects
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
};

