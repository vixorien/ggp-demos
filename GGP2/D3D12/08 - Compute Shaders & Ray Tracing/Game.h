#pragma once

#include "Mesh.h"
#include "GameEntity.h"
#include "Transform.h"
#include "Camera.h"
#include "Lights.h"
#include "Sky.h"
#include "BufferStructs.h"

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
	void CreateOutputTexture(unsigned int width, unsigned int height);

	// UI functions and variables
	void UINewFrame(float deltaTime);
	void BuildUI();
	bool showUIDemoWindow;

	// Pipeline
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

	// Compute pipeline
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSig;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePSO;
	Microsoft::WRL::ComPtr<ID3D12Resource> ComputeOutputTexture;
	D3D12_GPU_DESCRIPTOR_HANDLE ComputeOutputGPUHandle;
	unsigned int ComputeOutputHeapIndex;
	DrawData drawData;

	// Scene
	std::shared_ptr<FPSCamera> camera;
	std::vector<Sphere> spheres;
	std::shared_ptr<Sky> sky;

	// Other graphics data
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};
};

