#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "Material.h"
#include "SimpleShader.h"
#include "Lights.h"
#include "Sky.h"

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
	void LoadAssetsAndCreateEntities();

	// Helper for creating a solid color texture & SRV
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSolidColorTextureSRV(int width, int height, DirectX::XMFLOAT4 color);

	// General helpers for setup and drawing
	void RandomizeEntities();
	void GenerateLights();
	void DrawLightSources();

	// Camera for the 3D scene
	std::shared_ptr<FPSCamera> camera;

	// The sky box
	std::shared_ptr<Sky> sky;

	// Scene data
	std::vector<std::shared_ptr<Mesh>> meshes;
	std::vector<std::shared_ptr<Material>> materials;
	std::vector<std::shared_ptr<GameEntity>> entitiesRandom;
	std::vector<std::shared_ptr<GameEntity>> entitiesLineup;
	std::vector<std::shared_ptr<GameEntity>> entitiesGradient;
	std::vector<std::shared_ptr<GameEntity>>* currentScene;
	std::vector<Light> lights;
	
	// Overall lighting options
	DemoLightingOptions lightOptions;
	std::shared_ptr<Mesh> pointLightMesh;

	// Shaders (for shader swapping between pbr and non-pbr)
	std::shared_ptr<SimplePixelShader> pixelShader;
	std::shared_ptr<SimplePixelShader> pixelShaderPBR;

	// Shaders for solid color spheres
	std::shared_ptr<SimplePixelShader> solidColorPS;
	std::shared_ptr<SimpleVertexShader> vertexShader;
};

