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
#include "UIHelpers.h"

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
	void GenerateLights();
	void DrawLightSources();
	void DrawQuadAtLocation(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv, DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 scale = DirectX::XMFLOAT2(1, 1), DirectX::XMFLOAT3 pitchYawRoll = DirectX::XMFLOAT3(0, 0, 0));
	void RenderEntitiesWithToonShading(ToonShadingType toonMode, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> toonRamp = 0, bool offsetPositions = false, DirectX::XMFLOAT3 offset = DirectX::XMFLOAT3(0, 0, 0));
	void ResizePostProcessResources();

	// Camera for the 3D scene
	std::shared_ptr<FPSCamera> camera;

	// The sky box
	std::shared_ptr<Sky> sky;

	// Scene data
	std::shared_ptr<Mesh> quadMesh;
	std::vector<std::shared_ptr<Mesh>> meshes;
	std::vector<std::shared_ptr<Material>> materials;
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::vector<Light> lights;
	
	// Overall lighting options
	ToonOptions options;
	std::shared_ptr<Mesh> pointLightMesh;

	// Shaders for solid color spheres
	Microsoft::WRL::ComPtr<ID3D11PixelShader> solidColorPS;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;

	// D3D API objects
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

	// Toon shading -------------------------------
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> toonRamp1;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> toonRamp2;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> toonRamp3;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specularRamp;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> simpleTexturePS;


	// Outline rendering --------------------------
	
	// General post processing resources
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> ppRTV;		// Allows us to render to a texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ppSRV;		// Allows us to sample from the same texture
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clampSampler;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> fullscreenVS;
	void PreRender();
	void PostRender();

	// Sobel
	Microsoft::WRL::ComPtr<ID3D11PixelShader> sobelFilterPS;

	// Inside-out technique
	Microsoft::WRL::ComPtr<ID3D11VertexShader> insideOutVS;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> insideOutRasterState;
	void DrawOutlineInsideOut(std::shared_ptr<GameEntity> entity, std::shared_ptr<Camera> camera, float outlineSize);

	// Silhouette technique
	Microsoft::WRL::ComPtr<ID3D11PixelShader> silhouettePS;
	int silhouetteID;

	// Depth/normal technique
	Microsoft::WRL::ComPtr<ID3D11PixelShader> depthNormalOutlinePS;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneDepthRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneDepthSRV;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneNormalsRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneNormalsSRV;
};

