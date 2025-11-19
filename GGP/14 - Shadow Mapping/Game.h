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

	// General helpers for setup and drawing
	void GenerateLights();
	void DrawLightSources();

	void CreateShadowMapResources();
	void RenderShadowMap();

	// Camera for the 3D scene
	std::shared_ptr<FPSCamera> camera;

	// The sky box
	std::shared_ptr<Sky> sky;

	// Scene data
	std::vector<std::shared_ptr<Mesh>> meshes;
	std::vector<std::shared_ptr<Material>> materials;
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::vector<Light> lights;
	
	// Lighting
	DemoLightingOptions lightOptions;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> solidColorPS;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	std::shared_ptr<Mesh> pointLightMesh;

	// D3D API objects
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

	// Shadow resources and data
	DemoShadowOptions shadowOptions;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> shadowSampler;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> shadowRasterizer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> shadowVertexShader;

};

