#include "Game.h"
#include "Vertex.h"
#include "Input.h"

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
		hInstance,		   // The application's handle
		"DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true),			   // Show extra stats (fps) in title bar?
	basicPixelShader(0),
	fancyPixelShader(0),
	basicVertexShader(0),
	camera(0)
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
	// Since we've created the Mesh objects within this class (Game),
	// this is also where we should delete them!
	for (auto& m : meshes) { delete m; }
	for (auto& e : entities) { delete e; }

	delete camera;
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	//  - You'll be expanding and/or replacing these later
	LoadShaders();
	CreateBasicGeometry();
	
	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the camera
	camera = new Camera(0, 2, -15, 5.0f, 5.0f, XM_PIDIV4, (float)width / height, 0.01f, 100.0f, CameraProjectionType::Perspective);
}

// --------------------------------------------------------
// Loads shaders from compiled shader object (.cso) files
// --------------------------------------------------------
void Game::LoadShaders()
{
	basicVertexShader = new SimpleVertexShader(device, context, GetFullPathTo_Wide(L"VertexShader.cso").c_str());
	basicPixelShader = new SimplePixelShader(device, context, GetFullPathTo_Wide(L"PixelShader.cso").c_str());
	fancyPixelShader = new SimplePixelShader(device, context, GetFullPathTo_Wide(L"FancyPixelShader.cso").c_str());
}



// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	// Load 3D models	
	Mesh* cubeMesh = new Mesh(GetFullPathTo("../../../Assets/Models/cube.obj").c_str(), device);
	Mesh* cylinderMesh = new Mesh(GetFullPathTo("../../../Assets/Models/cylinder.obj").c_str(), device);
	Mesh* helixMesh = new Mesh(GetFullPathTo("../../../Assets/Models/helix.obj").c_str(), device);
	Mesh* sphereMesh = new Mesh(GetFullPathTo("../../../Assets/Models/sphere.obj").c_str(), device);
	Mesh* torusMesh = new Mesh(GetFullPathTo("../../../Assets/Models/torus.obj").c_str(), device);
	Mesh* quadMesh = new Mesh(GetFullPathTo("../../../Assets/Models/quad.obj").c_str(), device);
	Mesh* quad2sidedMesh = new Mesh(GetFullPathTo("../../../Assets/Models/quad_double_sided.obj").c_str(), device);

	meshes.push_back(cubeMesh);
	meshes.push_back(cylinderMesh);
	meshes.push_back(helixMesh);
	meshes.push_back(sphereMesh);
	meshes.push_back(torusMesh);
	meshes.push_back(quadMesh);
	meshes.push_back(quad2sidedMesh);

	// Create several different materials
	Material* matFancy = new Material(fancyPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1));
	Material* matWhite = new Material(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1));
	Material* matRed = new Material(basicPixelShader, basicVertexShader, XMFLOAT3(0.75f, 0, 0));
	Material* matPurple = new Material(basicPixelShader, basicVertexShader, XMFLOAT3(0.75f, 0, 0.6f));
	materials.push_back(matWhite);
	materials.push_back(matRed);
	materials.push_back(matPurple);

	// Create the game entities
	entities.push_back(new GameEntity(cubeMesh, matWhite));
	entities.push_back(new GameEntity(cylinderMesh, matRed));
	entities.push_back(new GameEntity(helixMesh, matPurple));
	entities.push_back(new GameEntity(sphereMesh, matFancy));
	entities.push_back(new GameEntity(torusMesh, matPurple));
	entities.push_back(new GameEntity(quadMesh, matRed));
	entities.push_back(new GameEntity(quad2sidedMesh, matWhite));

	// Adjust transforms
	entities[0]->GetTransform()->MoveAbsolute(-9, 0, 0);
	entities[1]->GetTransform()->MoveAbsolute(-6, 0, 0);
	entities[2]->GetTransform()->MoveAbsolute(-3, 0, 0);
	entities[3]->GetTransform()->MoveAbsolute(0, 0, 0);
	entities[4]->GetTransform()->MoveAbsolute(3, 0, 0);
	entities[5]->GetTransform()->MoveAbsolute(6, 0, 0);
	entities[6]->GetTransform()->MoveAbsolute(9, 0, 0);
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
	if (camera) camera->UpdateProjectionMatrix((float)width / height);
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
	for (auto& e : entities)
	{
		e->GetTransform()->Rotate(0, deltaTime, 0);
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
	// Background color (Cornflower Blue in this case) for clearing
	const float color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(
		depthStencilView.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);


	// Loop through the game entities and draw
	for (auto& e : entities)
	{
		// Set total time on this entity's material's pixel shader
		// Note: If the shader doesn't have this variable, nothing happens
		e->GetMaterial()->GetPixelShader()->SetFloat("time", totalTime);

		// Draw one entity
		e->Draw(context, camera);
	}


	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);

	// Due to the usage of a more sophisticated swap chain,
	// the render target must be re-bound after every call to Present()
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthStencilView.Get());
}