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
	void RenderEntitiesWithToonShading(int toonShadingType, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> toonRamp = 0, bool offsetPositions = false, DirectX::XMFLOAT3 offset = DirectX::XMFLOAT3(0, 0, 0));
	void ResizePostProcessResources();

	// Sprite batch helper methods
	void DrawUI();
	void DrawSpriteAtLocation(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv, DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 scale = DirectX::XMFLOAT2(1, 1), DirectX::XMFLOAT3 pitchYawRoll = DirectX::XMFLOAT3(0, 0, 0));
	void DrawTextAtLocation(const char* text, DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 scale = DirectX::XMFLOAT2(1, 1), DirectX::XMFLOAT3 pitchYawRoll = DirectX::XMFLOAT3(0, 0, 0));

	// Camera for the 3D scene
	std::shared_ptr<Camera> camera;

	// The sky box
	std::shared_ptr<Sky> sky;

	// Scene
	std::vector<std::shared_ptr<GameEntity>> entities;

	// Lights
	std::vector<Light> lights;
	DirectX::XMFLOAT3 ambientColor;
	int lightCount;
	bool freezeLightMovement;

	// Sprite batch resources
	std::shared_ptr<DirectX::SpriteBatch> spriteBatch;

	// General post processing resources
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> ppRTV;		// Allows us to render to a texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ppSRV;		// Allows us to sample from the same texture

	// Outline rendering --------------------------
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clampSampler;
	int outlineRenderingMode;
	void PreRender();
	void PostRender();

	// Inside-out technique
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> insideOutRasterState;
	void DrawOutlineInsideOut(std::shared_ptr<GameEntity> entity, std::shared_ptr<Camera> camera, float outlineSize);

	// Silhouette technique
	int silhouetteID;

	// Depth/normal technique
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneDepthRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneDepthSRV;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> sceneNormalsRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sceneNormalsSRV;
};

