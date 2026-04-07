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
	void ImageWithHover(unsigned int descriptorIndex, const ImVec2& size);
	bool showUIDemoWindow;

	// Pipeline
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> envPrevRootSig;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> envPrevPSO;

	// Scene
	int lightCount;
	std::vector<Light> lights;
	std::shared_ptr<FPSCamera> camera;
	std::vector<std::shared_ptr<GameEntity>> entities;
	std::vector<std::shared_ptr<Sky>> skies;

	unsigned int currentSky;
	bool directLightingEnabled;
	bool indirectLightingEnabled;

	bool previewIrradiance;
	std::shared_ptr<Mesh> envPreviewMesh;

	// Other graphics data
	D3D12_VIEWPORT viewport{};
	D3D12_RECT scissorRect{};
};

