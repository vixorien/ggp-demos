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
	ambientColor(0.1f, 0.15f, 0.25f)
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
		5.0f,				// Look speed
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
	// Load 3D models	
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/cube.obj").c_str(), device);
	std::shared_ptr<Mesh> cylinderMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/cylinder.obj").c_str(), device);
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/helix.obj").c_str(), device);
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/sphere.obj").c_str(), device);
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/torus.obj").c_str(), device);
	std::shared_ptr<Mesh> quadMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/quad.obj").c_str(), device);
	std::shared_ptr<Mesh> quad2sidedMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/quad_double_sided.obj").c_str(), device);

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
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
		rockSRV,
		rockNormalsSRV; 
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
		cushionSRV,
		cushionNormalsSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> 
		cobblestoneSRV, 
		cobblestoneNormalsSRV,
		cobblestoneSpecularSRV;

// Quick pre-processor macro for simplifying texture loading calls below
#define LoadTexture(path, srv) CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(path).c_str(), 0, srv.GetAddressOf());

	LoadTexture(L"../../../Assets/Textures/rock.png", rockSRV);
	LoadTexture(L"../../../Assets/Textures/rock_normals.png", rockNormalsSRV);
	LoadTexture(L"../../../Assets/Textures/cushion.png", cushionSRV);
	LoadTexture(L"../../../Assets/Textures/cushion_normals.png", cushionNormalsSRV);
	LoadTexture(L"../../../Assets/Textures/cobblestone.png", cobblestoneSRV);
	LoadTexture(L"../../../Assets/Textures/cobblestone_normals.png", cobblestoneNormalsSRV);
	LoadTexture(L"../../../Assets/Textures/cobblestone_specular.png", cobblestoneSpecularSRV);


	// Load shaders and create materials
	std::shared_ptr<SimpleVertexShader> basicVertexShader = std::make_shared<SimpleVertexShader>(device, context, FixPath(L"VertexShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> basicPixelShader = std::make_shared<SimplePixelShader>(device, context, FixPath(L"PixelShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> normalMapPS = std::make_shared<SimplePixelShader>(device, context, FixPath(L"NormalMapPS.cso").c_str());
	std::shared_ptr<SimplePixelShader> lightAndEnvMapPS = std::make_shared<SimplePixelShader>(device, context, FixPath(L"LightingAndEnvMapPS.cso").c_str());
	std::shared_ptr<SimplePixelShader> envMapOnlyPS = std::make_shared<SimplePixelShader>(device, context, FixPath(L"EnvMapOnlyPS.cso").c_str());
	std::shared_ptr<SimpleVertexShader> skyVS = std::make_shared<SimpleVertexShader>(device, context, FixPath(L"SkyVS.cso").c_str());
	std::shared_ptr<SimplePixelShader> skyPS = std::make_shared<SimplePixelShader>(device, context, FixPath(L"SkyPS.cso").c_str());

	// Create the sky
	sky = std::make_shared<Sky>(
		FixPath(L"../../../Assets/Skies/Clouds Blue/right.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/left.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/up.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/down.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/front.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/back.png").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		sampler,
		device,
		context);


	// Create basic materials (no normal maps) ---------------------
	std::shared_ptr<Material> matRock = std::make_shared<Material>(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRock->AddSampler("BasicSampler", sampler);
	matRock->AddTextureSRV("SurfaceTexture", rockSRV);
	
	std::shared_ptr<Material> matCushion = std::make_shared<Material>(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushion->AddSampler("BasicSampler", sampler);
	matCushion->AddTextureSRV("SurfaceTexture", cushionSRV);

	std::shared_ptr<Material> matCobblestone = std::make_shared<Material>(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestone->AddSampler("BasicSampler", sampler);
	matCobblestone->AddTextureSRV("SurfaceTexture", cobblestoneSRV);
	matCobblestone->AddTextureSRV("SpecularMap", cobblestoneSpecularSRV);


	// Create normal mapped materials ---------------------
	std::shared_ptr<Material> matRockNormalMap = std::make_shared<Material>(normalMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRockNormalMap->AddSampler("BasicSampler", sampler);
	matRockNormalMap->AddTextureSRV("SurfaceTexture", rockSRV);
	matRockNormalMap->AddTextureSRV("NormalMap", rockNormalsSRV);

	std::shared_ptr<Material> matCushionNormalMap = std::make_shared<Material>(normalMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushionNormalMap->AddSampler("BasicSampler", sampler);
	matCushionNormalMap->AddTextureSRV("SurfaceTexture", cushionSRV);
	matCushionNormalMap->AddTextureSRV("NormalMap", cushionNormalsSRV);

	std::shared_ptr<Material> matCobblestoneNormalMap = std::make_shared<Material>(normalMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestoneNormalMap->AddSampler("BasicSampler", sampler);
	matCobblestoneNormalMap->AddTextureSRV("SurfaceTexture", cobblestoneSRV);
	matCobblestoneNormalMap->AddTextureSRV("NormalMap", cobblestoneNormalsSRV);
	matCobblestoneNormalMap->AddTextureSRV("SpecularMap", cobblestoneSpecularSRV);


	// Create normal mapped & environment mapped materials ---------------------
	std::shared_ptr<Material> matRockLitEnvMap = std::make_shared<Material>(lightAndEnvMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRockLitEnvMap->AddSampler("BasicSampler", sampler);
	matRockLitEnvMap->AddTextureSRV("SurfaceTexture", rockSRV);
	matRockLitEnvMap->AddTextureSRV("NormalMap", rockNormalsSRV);
	matRockLitEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	std::shared_ptr<Material> matCushionLitEnvMap = std::make_shared<Material>(lightAndEnvMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushionLitEnvMap->AddSampler("BasicSampler", sampler);
	matCushionLitEnvMap->AddTextureSRV("SurfaceTexture", cushionSRV);
	matCushionLitEnvMap->AddTextureSRV("NormalMap", cushionNormalsSRV);
	matCushionLitEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	std::shared_ptr<Material> matCobblestoneLitEnvMap = std::make_shared<Material>(lightAndEnvMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestoneLitEnvMap->AddSampler("BasicSampler", sampler);
	matCobblestoneLitEnvMap->AddTextureSRV("SurfaceTexture", cobblestoneSRV);
	matCobblestoneLitEnvMap->AddTextureSRV("NormalMap", cobblestoneNormalsSRV);
	matCobblestoneLitEnvMap->AddTextureSRV("SpecularMap", cobblestoneSpecularSRV);
	matCobblestoneLitEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	// Create environment mapped only materials ---------------------
	std::shared_ptr<Material> matRockEnvMap = std::make_shared<Material>(envMapOnlyPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRockEnvMap->AddSampler("BasicSampler", sampler);
	matRockEnvMap->AddTextureSRV("NormalMap", rockNormalsSRV);
	matRockEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	std::shared_ptr<Material> matCushionEnvMap = std::make_shared<Material>(envMapOnlyPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushionEnvMap->AddSampler("BasicSampler", sampler);
	matCushionEnvMap->AddTextureSRV("NormalMap", cushionNormalsSRV);
	matCushionEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	std::shared_ptr<Material> matCobblestoneEnvMap = std::make_shared<Material>(envMapOnlyPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestoneEnvMap->AddSampler("BasicSampler", sampler);
	matCobblestoneEnvMap->AddTextureSRV("NormalMap", cobblestoneNormalsSRV);
	matCobblestoneEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

		

	// Create three sets of entities - with and without normal maps and env map
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matRock));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matRock));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCushion));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCushion));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCobblestone));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCobblestone));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matRockNormalMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matRockNormalMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCushionNormalMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCushionNormalMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCobblestoneNormalMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCobblestoneNormalMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matRockLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matRockLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCushionLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCushionLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCobblestoneLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCobblestoneLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matRockEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matRockEnvMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCushionEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCushionEnvMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCobblestoneEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCobblestoneEnvMap));

	// Scale all the cubes
	for (int i = 0; i < entities.size(); i += 2)
		entities[i]->GetTransform()->Scale(2, 2, 2);

	// Line up the entities like so:
	//
	//  c  s  c  s  c  s  <-- Regular
	//
	//  c  s  c  s  c  s  <-- Normal mapped
	//
	//  c  s  c  s  c  s  <-- Lit & Environment mapped
	//
	//  c  s  c  s  c  s  <-- Environment mapped only
	//
	int i = 0;
	for (float y = 4.5; y >= -4.5; y -= 3)
	{
		for (float x = -7.5f; x <= 7.5f; x += 3)
		{
			entities[i++]->GetTransform()->MoveAbsolute(x, y, 0);
		}
	}

	// Create lights - Must respect the
	// max lights defined in the pixel shader!
	// Note: directions are currently being normalized in the shader
	Light dirLight1 = {};
	dirLight1.Color = XMFLOAT3(0.8f, 0.9f, 1);
	dirLight1.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight1.Intensity = 1.0f;
	dirLight1.Direction = XMFLOAT3(1, 0, 0);

	Light dirLight2 = {};
	dirLight2.Color = XMFLOAT3(0.8f, 0.9f, 1);
	dirLight2.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight2.Intensity = 1.0f;
	dirLight2.Direction = XMFLOAT3(-0.25f, -1, 0.75f);

	Light dirLight3 = {};
	dirLight3.Color = XMFLOAT3(0.8f, 0.9f, 1);
	dirLight3.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight3.Intensity = 1.0f;
	dirLight3.Direction = XMFLOAT3(-1, 1,-0.5f);

	// Point lights set to zero intensity for this demo
	// Alternatively, they could be removed entirely
	Light pointLight1 = {};
	pointLight1.Color = XMFLOAT3(1,1,1);
	pointLight1.Type = LIGHT_TYPE_POINT;
	pointLight1.Intensity = 0.0f;
	pointLight1.Position = XMFLOAT3(-1.5f, 0, 0);
	pointLight1.Range = 10.0f;

	Light pointLight2 = {};
	pointLight2.Color = XMFLOAT3(1, 1, 1);
	pointLight2.Type = LIGHT_TYPE_POINT;
	pointLight2.Intensity = 0.0f;
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
		e->GetTransform()->Rotate(0, deltaTime * 0.25f, 0);
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
		const float bgColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Black
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

	// Draw the sky after all regular entities
	sky->Draw(camera);


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