#pragma once

#include "Mesh.h"
#include "GameEntity.h"
#include "Transform.h"
#include "Camera.h"
#include "Lights.h"
#include "Sky.h"

#include "ImGui/imgui.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

enum RenderTargetType
{
	GBUFFER_ALBEDO,
	GBUFFER_NORMALS,
	GBUFFER_MATERIAL,
	GBUFFER_DEPTH,
	SCENE,
	LIGHT_BUFFER,

	// Count is always the last one!
	RENDER_TARGET_TYPE_COUNT
};

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
	void CreateRootSigAndPipelineState();
	void CreateGeometry();
	void GenerateLights();

	// UI functions and variables
	void UINewFrame(float deltaTime);
	void BuildUI();
	void ImageWithHover(D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle, const ImVec2& size);
	bool showUIDemoWindow;

	// Pipeline
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

	// Scene
	int lightCount;
	std::vector<Light> lights;
	std::shared_ptr<FPSCamera> camera;
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::shared_ptr<Sky> sky;

	// Other graphics data
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};

	// Deferred
	bool deferredLighting = false;

	// GBuffer
	static const unsigned int MaxRenderTargets = 10;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	TextureDetails RenderTargets[RENDER_TARGET_TYPE_COUNT]{};

	// Going to assume the same root signature is compatible with this PSO
	Microsoft::WRL::ComPtr<ID3D12PipelineState> fullScreenTexturePSO;
};

