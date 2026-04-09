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

	// Multiple render targets
	const unsigned int MaxRenderTargets = 10;
	const unsigned int AlbedoRT = 0;
	const unsigned int NormalRT = 1;
	const unsigned int MaterialRT = 2;
	const unsigned int DepthRT = 3;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	TextureDetails RenderTargets[4];
};

