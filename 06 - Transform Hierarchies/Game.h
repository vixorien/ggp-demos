#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"

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
	void LoadShaders();
	void CreateGeometry();

	// UI functions
	void UINewFrame(float deltaTime);
	void BuildUI();
	void CameraUI(std::shared_ptr<Camera> cam);
	void EntityUI(std::shared_ptr<GameEntity> entity);

	// Should the ImGui demo window be shown?
	bool showUIDemoWindow;

	// Camera for the 3D scene
	std::shared_ptr<Camera> camera;

	// Vectors for scene objects
	std::vector<std::shared_ptr<Mesh>> meshes;
	std::vector<std::shared_ptr<GameEntity>> entities;

	// Constant buffer to hold data being sent to variables in the vertex shader
	// - This represents a buffer on the GPU, though it's just a C++ object
	Microsoft::WRL::ComPtr<ID3D11Buffer> vsConstantBuffer;

	// Shaders and shader-related constructs
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

};

