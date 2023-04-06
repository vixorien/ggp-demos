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

	// Helper for creating a solid color texture & SRV
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSolidColorTextureSRV(int width, int height, DirectX::XMFLOAT4 color);

	// General helpers for setup and drawing
	void RandomizeEntities();
	void GenerateLights();
	void DrawLightSources();
	void DrawUI();

	// Camera for the 3D scene
	std::shared_ptr<Camera> camera;

	// The sky box
	std::shared_ptr<Sky> sky;

	// Storage for scene data
	std::vector<std::shared_ptr<Mesh>> meshes;
	std::vector<std::shared_ptr<Material>> materials;
	std::vector<std::shared_ptr<GameEntity>> entitiesRandom;
	std::vector<std::shared_ptr<GameEntity>> entitiesLineup;
	std::vector<std::shared_ptr<GameEntity>> entitiesGradient;
	std::vector<std::shared_ptr<GameEntity>>* currentScene;

	// Lights
	std::vector<Light> lights;
	DirectX::XMFLOAT3 ambientColor;
	int lightCount;
	bool gammaCorrection;
	bool useAlbedoTexture;
	bool useMetalMap;
	bool useNormalMap;
	bool useRoughnessMap;
	bool usePBR;
	bool freezeLightMovement;
	bool drawLights;
	bool showSkybox;
	std::shared_ptr<Mesh> lightMesh;

	// Shaders (for shader swapping between pbr and non-pbr)
	std::shared_ptr<SimplePixelShader> pixelShader;
	std::shared_ptr<SimplePixelShader> pixelShaderPBR;

	// Shaders for solid color spheres
	std::shared_ptr<SimplePixelShader> solidColorPS;
	std::shared_ptr<SimpleVertexShader> vertexShader;

	// UI functions
	void UINewFrame(float deltaTime);
	void BuildUI();
	void CameraUI(std::shared_ptr<Camera> cam);
	void EntityUI(std::shared_ptr<GameEntity> entity);
	void MaterialUI(std::shared_ptr<Material> material);
	void LightUI(Light& light);

	// Should the ImGui demo window be shown?
	bool showUIDemoWindow;
};

