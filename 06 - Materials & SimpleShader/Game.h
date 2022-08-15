#pragma once

#include "DXCore.h"
#include "Mesh.h"
#include "GameEntity.h"
#include "Camera.h"
#include "Material.h"
#include "SimpleShader.h"

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

	// Camera for the 3D scene
	std::shared_ptr<Camera> camera;

	// A vector to hold entities
	// - This makes things easy to draw and clean up, too!
	std::vector<std::shared_ptr<GameEntity>> entities;

	// Shaders and shader-related constructs
	std::shared_ptr<SimplePixelShader> basicPixelShader;
	std::shared_ptr<SimplePixelShader> fancyPixelShader;
	std::shared_ptr<SimpleVertexShader> basicVertexShader;

};

