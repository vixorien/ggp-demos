#pragma once

#include "DXCore.h"
#include "Mesh.h"

#include <vector>
#include <DirectXMath.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects
#include <memory>

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

	// Should the ImGui demo window be shown?
	bool showUIDemoWindow;

	// A vector to hold any number of meshes
	// - This makes things easy to draw and clean up, too!
	std::vector<std::shared_ptr<Mesh>> meshes;
	
	// Shaders and shader-related constructs
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;

};

