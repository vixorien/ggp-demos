#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Helpers.h"

// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore(
		hInstance,			// The application's handle
		L"DirectX Game",	// Text for the window's title bar (as a wide-character string)
		1280,				// Width of the window's client area
		720,				// Height of the window's client area
		false,				// Sync the framerate to the monitor refresh? (lock framerate)
		true),				// Show extra stats (fps) in title bar?
	ambientColor(0.1f, 0.1f, 0.25f)
{

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Call delete or delete[] on any objects or arrays you've
	// created using new or new[] within this class
	// - Note: this is unnecessary if using smart pointers

	// Call Release() on any Direct3D objects made within this class
	// - Note: this is unnecessary for D3D objects stored in ComPtrs
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	LoadAssetsAndCreateEntities();
	
	// Set initial graphics API state
	//  - These settings persist until we change them
	{
		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// Create the camera
	camera = std::make_shared<Camera>(
		0.0f, 0.0f, -15.0f,	// Position
		5.0f,				// Move speed
		0.002f,				// Look speed
		XM_PIDIV4,			// Field of view
		(float)windowWidth / windowHeight,  // Aspect ratio
		0.01f,				// Near clip
		100.0f,				// Far clip
		CameraProjectionType::Perspective);
}


// --------------------------------------------------------
// Loads all necessary assets and creates various entities
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Load shaders and create materials
	std::shared_ptr<SimpleVertexShader> basicVertexShader = std::make_shared<SimpleVertexShader>(device, context, FixPath(L"VertexShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> basicPixelShader = std::make_shared<SimplePixelShader>(device, context, FixPath(L"PixelShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> fancyPixelShader = std::make_shared<SimplePixelShader>(device, context, FixPath(L"FancyPixelShader.cso").c_str());

	// Create several different materials
	std::shared_ptr<Material> matFancy = std::make_shared<Material>(fancyPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 1.0f);
	std::shared_ptr<Material> matWhite = std::make_shared<Material>(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.15f);

	// Load 3D models	
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/cube.obj").c_str(), device);
	std::shared_ptr<Mesh> cylinderMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/cylinder.obj").c_str(), device);
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/helix.obj").c_str(), device);
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/sphere.obj").c_str(), device);
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/torus.obj").c_str(), device);
	std::shared_ptr<Mesh> quadMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/quad.obj").c_str(), device);
	std::shared_ptr<Mesh> quad2sidedMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/quad_double_sided.obj").c_str(), device);

	// Create the game entities
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matWhite));
	entities.push_back(std::make_shared<GameEntity>(cylinderMesh, matWhite));
	entities.push_back(std::make_shared<GameEntity>(helixMesh, matWhite));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matWhite));
	entities.push_back(std::make_shared<GameEntity>(torusMesh, matWhite));
	entities.push_back(std::make_shared<GameEntity>(quadMesh, matWhite));
	entities.push_back(std::make_shared<GameEntity>(quad2sidedMesh, matWhite));

	// Adjust transforms
	entities[0]->GetTransform()->MoveAbsolute(-9, 0, 0);
	entities[1]->GetTransform()->MoveAbsolute(-6, 0, 0);
	entities[2]->GetTransform()->MoveAbsolute(-3, 0, 0);
	entities[3]->GetTransform()->MoveAbsolute(0, 0, 0);
	entities[4]->GetTransform()->MoveAbsolute(3, 0, 0);
	entities[5]->GetTransform()->MoveAbsolute(6, 0, 0);
	entities[6]->GetTransform()->MoveAbsolute(9, 0, 0);

	// Create lights - Must respect the
	// max lights defined in the pixel shader!
	Light dirLight1 = {};
	dirLight1.Color = XMFLOAT3(1, 0, 0);
	dirLight1.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight1.Intensity = 1.0f;
	dirLight1.Direction = XMFLOAT3(1, 0, 0);

	Light dirLight2 = {};
	dirLight2.Color = XMFLOAT3(0, 1, 0);
	dirLight2.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight2.Intensity = 1.0f;
	dirLight2.Direction = XMFLOAT3(0, -1, 0);

	Light dirLight3 = {};
	dirLight3.Color = XMFLOAT3(0,0,1);
	dirLight3.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight3.Intensity = 1.0f;
	dirLight3.Direction = XMFLOAT3(-1, 1,-0.5f); // Should be normalized (shader is doing it for now)

	Light pointLight1 = {};
	pointLight1.Color = XMFLOAT3(1,1,1);
	pointLight1.Type = LIGHT_TYPE_POINT;
	pointLight1.Intensity = 1.0f;
	pointLight1.Position = XMFLOAT3(-1.5f, 0, 0);
	pointLight1.Range = 10.0f;

	Light pointLight2 = {};
	pointLight2.Color = XMFLOAT3(1, 1, 1);
	pointLight2.Type = LIGHT_TYPE_POINT;
	pointLight2.Intensity = 0.5f;
	pointLight2.Position = XMFLOAT3(1.5f, 0, 0);
	pointLight2.Range = 10.0f;

	// Add all lights to the list
	lights.push_back(dirLight1);
	lights.push_back(dirLight2);
	lights.push_back(dirLight3);
	lights.push_back(pointLight1);
	lights.push_back(pointLight2);
}


// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Update the camera's projection to match the new aspect ratio
	if (camera) camera->UpdateProjectionMatrix((float)windowWidth / windowHeight);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Example input checking: Quit if the escape key is pressed
	if (Input::GetInstance().KeyDown(VK_ESCAPE))
		Quit();

	// Spin the 3D models
	float offset = 0.0f;
	for (auto& e : entities)
	{
		e->GetTransform()->Rotate(0, deltaTime, 0);

		// Bob up and down
		XMFLOAT3 pos = e->GetTransform()->GetPosition();
		e->GetTransform()->SetPosition(pos.x, sin(totalTime * 2.0f + offset) * 2.0f, pos.z);
		offset += 0.31415f;
	}

	// Rotate and scale the first one some more
	float scale = (float)sin(totalTime * 5) * 0.5f + 1.0f;
	entities[0]->GetTransform()->SetScale(scale, scale, scale);
	entities[0]->GetTransform()->Rotate(0, 0, deltaTime * 1.0f);

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
		// Clear the back buffer (erases what's on the screen)
		const float bgColor[4] = { 0.4f, 0.6f, 0.75f, 1.0f }; // Cornflower Blue
		context->ClearRenderTargetView(backBufferRTV.Get(), bgColor);

		// Clear the depth buffer (resets per-pixel occlusion information)
		context->ClearDepthStencilView(depthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}


	// DRAW geometry
	// Loop through the game entities and draw each one
	for (auto& e : entities)
	{
		// Set total time on this entity's material's pixel shader
		// Note: If the shader doesn't have this variable, nothing happens
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetFloat("time", totalTime);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());

		// Draw one entity
		e->Draw(context, camera);
	}


	// Frame END
	// - These should happen exactly ONCE PER FRAME
	// - At the very end of the frame (after drawing *everything*)
	{
		// Present the back buffer to the user
		//  - Puts the results of what we've drawn onto the window
		//  - Without this, the user never sees anything
		bool vsyncNecessary = vsync || !deviceSupportsTearing || isFullscreen;
		swapChain->Present(
			vsyncNecessary ? 1 : 0,
			vsyncNecessary ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Must re-bind buffers after presenting, as they become unbound
		context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthBufferDSV.Get());
	}
}