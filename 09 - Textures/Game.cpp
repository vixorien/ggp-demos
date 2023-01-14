#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Helpers.h"

#include "WICTextureLoader.h"

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
	// Create a sampler state for texture sampling options
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; // What happens outside the 0-1 uv range?
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;		// How do we handle sampling "between" pixels?
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());

	// Load textures
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> brokenTilesSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> brokenTilesSpecularSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tilesSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> tilesSpecularSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobblestoneSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobblestoneSpecularSRV;

	CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(L"../../../Assets/Textures/brokentiles.png").c_str(), 0, brokenTilesSRV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(L"../../../Assets/Textures/brokentiles_specular.png").c_str(), 0, brokenTilesSpecularSRV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(L"../../../Assets/Textures/tiles.png").c_str(), 0, tilesSRV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(L"../../../Assets/Textures/tiles_specular.png").c_str(), 0, tilesSpecularSRV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(L"../../../Assets/Textures/cobblestone.png").c_str(), 0, cobblestoneSRV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(L"../../../Assets/Textures/cobblestone_specular.png").c_str(), 0, cobblestoneSpecularSRV.GetAddressOf());


	// Load shaders and create materials
	std::shared_ptr<SimpleVertexShader> basicVertexShader = std::make_shared<SimpleVertexShader>(device, context, FixPath(L"VertexShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> basicPixelShader = std::make_shared<SimplePixelShader>(device, context, FixPath(L"PixelShader.cso").c_str());

	// Create several different materials
	std::shared_ptr<Material> matTiles = std::make_shared<Material>(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, XMFLOAT2(5, 5));
	matTiles->AddSampler("BasicSampler", sampler);
	matTiles->AddTextureSRV("SurfaceTexture", tilesSRV);
	matTiles->AddTextureSRV("SpecularMap", tilesSpecularSRV);
	
	std::shared_ptr<Material> matBrokenTiles = std::make_shared<Material>(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, XMFLOAT2(2, 2));
	matBrokenTiles->AddSampler("BasicSampler", sampler);
	matBrokenTiles->AddTextureSRV("SurfaceTexture", brokenTilesSRV);
	matBrokenTiles->AddTextureSRV("SpecularMap", brokenTilesSpecularSRV);

	std::shared_ptr<Material> matCobblestone = std::make_shared<Material>(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f);
	matCobblestone->AddSampler("BasicSampler", sampler);
	matCobblestone->AddTextureSRV("SurfaceTexture", cobblestoneSRV);
	matCobblestone->AddTextureSRV("SpecularMap", cobblestoneSpecularSRV);

	// Load 3D models	
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/cube.obj").c_str(), device);
	std::shared_ptr<Mesh> cylinderMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/cylinder.obj").c_str(), device);
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/helix.obj").c_str(), device);
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/sphere.obj").c_str(), device);
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/torus.obj").c_str(), device);
	std::shared_ptr<Mesh> quadMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/quad.obj").c_str(), device);
	std::shared_ptr<Mesh> quad2sidedMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/quad_double_sided.obj").c_str(), device);

	// Create the game entities
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCobblestone));
	entities.push_back(std::make_shared<GameEntity>(cylinderMesh, matBrokenTiles));
	entities.push_back(std::make_shared<GameEntity>(helixMesh, matBrokenTiles));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matTiles));
	entities.push_back(std::make_shared<GameEntity>(torusMesh, matTiles));
	entities.push_back(std::make_shared<GameEntity>(quadMesh, matTiles));
	entities.push_back(std::make_shared<GameEntity>(quad2sidedMesh, matBrokenTiles));

	// Adjust transforms
	entities[0]->GetTransform()->Scale(2, 2, 2);
	entities[0]->GetTransform()->MoveAbsolute(-9, 0, 0);
	entities[1]->GetTransform()->MoveAbsolute(-6, 0, 0);
	entities[2]->GetTransform()->MoveAbsolute(-3, 0, 0);
	entities[3]->GetTransform()->MoveAbsolute(0, 0, 0);
	entities[4]->GetTransform()->MoveAbsolute(3, 0, 0);
	entities[5]->GetTransform()->MoveAbsolute(6, 0, 0);
	entities[6]->GetTransform()->MoveAbsolute(9, 0, 0);

	// Create lights - Must respect the
	// max lights defined in the pixel shader!
	// Note: directions are currently being normalized in the shader
	Light dirLight1 = {};
	dirLight1.Color = XMFLOAT3(1, 1, 1);
	dirLight1.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight1.Intensity = 1.0f;
	dirLight1.Direction = XMFLOAT3(1, 0.5f, 0.5f);

	Light dirLight2 = {};
	dirLight2.Color = XMFLOAT3(1, 1, 1);
	dirLight2.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight2.Intensity = 1.0f;
	dirLight2.Direction = XMFLOAT3(-0.25f, -1, 0.75f);

	Light dirLight3 = {};
	dirLight3.Color = XMFLOAT3(1, 1, 1);
	dirLight3.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight3.Intensity = 1.0f;
	dirLight3.Direction = XMFLOAT3(-1, 1,-0.5f);

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
		e->GetTransform()->Rotate(0, deltaTime * 0.1f, 0);
	}

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
	// Loop through the game entities and draw
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