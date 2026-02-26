#pragma once

#include "Mesh.h"
#include "GameEntity.h"
#include "Transform.h"
#include "Camera.h"
#include "Lights.h"
#include "Sky.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

struct RWTextureDetails
{
	std::string name;
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle;
	unsigned int DescriptorHeapIndex;
	unsigned int Width;
	unsigned int Height;
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
	RWTextureDetails CreateRWTexture(std::string name, unsigned int width, unsigned int height);
	void GenerateLights();

	// UI functions and variables
	void UINewFrame(float deltaTime);
	void BuildUI();
	bool showUIDemoWindow;

	// Pipeline
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

	// Compute pipeline
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSig;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> noiseGenPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> textureGenPSO;
	std::vector<RWTextureDetails> RWTextures;

	// Scene
	int lightCount;
	std::vector<Light> lights;
	std::shared_ptr<FPSCamera> camera;
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::shared_ptr<Sky> sky;

	// Other graphics data
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};
};

