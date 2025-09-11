#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "UIHelpers.h"
#include "AssetPath.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include <DirectXMath.h>

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// Helper macro for getting a float between min and max
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// For the DirectX Math library
using namespace DirectX;

// --------------------------------------------------------
// Called once per program, after the window and graphics API
// are initialized but before the game loop begins
// --------------------------------------------------------
void Game::Initialize()
{
	// Initialize ImGui itself & platform/renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(Window::Handle());
	ImGui_ImplDX11_Init(Graphics::Device.Get(), Graphics::Context.Get());
	ImGui::StyleColorsDark();

	// Seed random
	srand((unsigned int)time(0));

	// Set up entities
	LoadAssetsAndCreateEntities();

	// Set initial graphics API state
	//  - These settings persist until we change them
	//  - Some of these, like the primitive topology & input layout, probably won't change
	//  - Others, like setting shaders, will need to be moved elsewhere later
	{
		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// Options for fog
	fogOptions =
	{
		.FogType = 1,
		.FogColor = XMFLOAT3(0.5f, 0.5f, 0.5f),
		.FogStartDistance = 20.0f,
		.FogEndDistance = 60.0f,
		.FogDensity = 0.02f,
		.HeightBasedFog = false,
		.FogHeight = 10.0f,
		.FogVerticalDensity = 0.5f,
		.MatchBackgroundToFog = false
	};

	// Create the camera
	camera = std::make_shared<FPSCamera>(
		XMFLOAT3(0.0f, 2.0f, -15.0f),	// Position
		5.0f,					// Move speed
		0.002f,					// Look speed
		XM_PIDIV4,				// Field of view
		Window::AspectRatio(),  // Aspect ratio
		0.01f,					// Near clip
		300.0f,					// Far clip
		CameraProjectionType::Perspective);
}


// --------------------------------------------------------
// Clean up memory or objects created by this class
// 
// Note: Using smart pointers means there probably won't
//       be much to manually clean up here!
// --------------------------------------------------------
Game::~Game()
{
	// ImGui clean up
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}


// --------------------------------------------------------
// Loads assets and creates the geometry we're going to draw
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Load shaders
	std::shared_ptr<SimpleVertexShader> basicVertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"VertexShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> basicPixelShader = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShader.cso").c_str());

	// Load 3D models	
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>("Cube", FixPath(AssetPath + L"Meshes/cube.obj").c_str());
	std::shared_ptr<Mesh> cylinderMesh = std::make_shared<Mesh>("Cylinder", FixPath(AssetPath + L"Meshes/cylinder.obj").c_str());
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>("Helix", FixPath(AssetPath + L"Meshes/helix.obj").c_str());
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>("Sphere", FixPath(AssetPath + L"Meshes/sphere.obj").c_str());
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>("Torus", FixPath(AssetPath + L"Meshes/torus.obj").c_str());
	std::shared_ptr<Mesh> quadMesh = std::make_shared<Mesh>("Quad", FixPath(AssetPath + L"Meshes/quad.obj").c_str());
	std::shared_ptr<Mesh> quad2sidedMesh = std::make_shared<Mesh>("Double-Sided Quad", FixPath(AssetPath + L"Meshes/quad_double_sided.obj").c_str());

	// Add all meshes to vector
	meshes.insert(meshes.end(), { cubeMesh, cylinderMesh, helixMesh, sphereMesh, torusMesh, quadMesh, quad2sidedMesh });

	// Create several different materials
	std::shared_ptr<Material> matSmooth = std::make_shared<Material>("Smooth", basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.02f);
	
	// Add all materials to vector
	materials.insert(materials.end(), { matSmooth });

	std::shared_ptr<GameEntity> floor = std::make_shared<GameEntity>(cubeMesh, matSmooth);
	floor->GetTransform()->SetScale(300, 25, 300);
	floor->GetTransform()->SetPosition(0, -25, 0);
	entities.push_back(floor);

	float spacing = 25.0f;
	for (int x = -5; x <= 5; x += 1)
		for (int z = -5; z <= 5; z += 1)
		{
			std::shared_ptr<Material> matRand = std::make_shared<Material>(
				"Color", 
				basicPixelShader, 
				basicVertexShader, 
				XMFLOAT3(RandomRange(0.1f, 1.0f), RandomRange(0.1f, 1.0f), RandomRange(0.1f, 1.0f)), 
				0.02f);


			std::shared_ptr<GameEntity> cube = std::make_shared<GameEntity>(cubeMesh, matRand);
			float height = 10;
			float scale = RandomRange(1.0f, 3.0f);
			cube->GetTransform()->SetScale(scale, height + RandomRange(-2.0f, 2.0f), scale);
			cube->GetTransform()->SetPosition(x * spacing - spacing /2, height / 2, z * spacing);

			entities.push_back(cube);
		}


	// Create lights - Must respect the
	// max lights defined in the pixel shader!
	Light dirLight1 = {};
	dirLight1.Color = XMFLOAT3(1, 1, 1);
	dirLight1.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight1.Intensity = 1.0f;
	dirLight1.Direction = XMFLOAT3(1, 0, 1);

	Light dirLight2 = {};
	dirLight2.Color = XMFLOAT3(1, 1, 1);
	dirLight2.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight2.Intensity = 0.5f;
	dirLight2.Direction = XMFLOAT3(-1, -1, 0);

	Light dirLight3 = {};
	dirLight3.Color = XMFLOAT3(1, 1, 1);
	dirLight3.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight3.Intensity = 0.1f;
	dirLight3.Direction = XMFLOAT3(-1, 1, -0.5f); // Will be normalized below

	// Add all lights to the list
	lights.push_back(dirLight1);
	lights.push_back(dirLight2);
	lights.push_back(dirLight3);

	// Normalize directions of all non-point lights
	for (int i = 0; i < lights.size(); i++)
		if (lights[i].Type != LIGHT_TYPE_POINT)
			XMStoreFloat3(
				&lights[i].Direction, 
				XMVector3Normalize(XMLoadFloat3(&lights[i].Direction))
			);
}


// --------------------------------------------------------
// Handle resizing to match the new window size
//  - Eventually, we'll want to update our 3D camera
// --------------------------------------------------------
void Game::OnResize()
{
	// Update the camera's projection to match the new aspect ratio
	if (camera) camera->UpdateProjectionMatrix(Window::AspectRatio());
}


// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Set up the new frame for the UI, then build
	// this frame's interface.  Note that the building
	// of the UI could happen at any point during update.
	UINewFrame(deltaTime);
	BuildUI(camera, meshes, entities, materials, lights, ambientColor, fogOptions);

	// Example input checking: Quit if the escape key is pressed
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();

	// Update the camera this frame
	camera->Update(deltaTime);
}


// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Frame START
	// - These things should happen ONCE PER FRAME
	// - At the beginning of Game::Draw() before drawing *anything*
	{
		// Clear the back buffer (erase what's on screen) and depth buffer
		float color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };
		if (fogOptions.MatchBackgroundToFog)
		{
			color[0] = fogOptions.FogColor.x;
			color[1] = fogOptions.FogColor.y;
			color[2] = fogOptions.FogColor.z;
		}
		Graphics::Context->ClearRenderTargetView(Graphics::BackBufferRTV.Get(), color);
		Graphics::Context->ClearDepthStencilView(Graphics::DepthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	// DRAW geometry
	// Loop through the game entities and draw each one
	// - Note: A constant buffer has already been bound to
	//   the vertex shader stage of the pipeline (see Init above)
	for (auto& e : entities)
	{
		// Set total time on this entity's material's pixel shader
		// Note: If the shader doesn't have this variable, nothing happens
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetFloat("time", totalTime);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());

		// Fog stuff
		ps->SetFloat("farClipDistance", camera->GetFarClip());
		ps->SetFloat3("fogColor", fogOptions.FogColor);
		ps->SetFloat("fogDensity", fogOptions.FogDensity);
		ps->SetFloat("fogStartDist", fogOptions.FogStartDistance);
		ps->SetFloat("fogEndDist", fogOptions.FogEndDistance);
		ps->SetInt("fogType", fogOptions.FogType);
		ps->SetInt("heightBasedFog", fogOptions.HeightBasedFog);
		ps->SetFloat("fogVerticalDensity", fogOptions.FogVerticalDensity);
		ps->SetFloat("fogHeight", fogOptions.FogHeight);

		// Draw one entity
		e->Draw(camera);
	}

	// Frame END
	// - These should happen exactly ONCE PER FRAME
	// - At the very end of the frame (after drawing *everything*)
	{
		// Draw the UI after everything else
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		// Present at the end of the frame
		bool vsync = Graphics::VsyncState();
		Graphics::SwapChain->Present(
			vsync ? 1 : 0,
			vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Re-bind back buffer and depth buffer after presenting
		Graphics::Context->OMSetRenderTargets(
			1,
			Graphics::BackBufferRTV.GetAddressOf(),
			Graphics::DepthBufferDSV.Get());
	}
}
