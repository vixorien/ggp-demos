#include "Game.h"
#include "Vertex.h"
#include "Input.h"

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
		hInstance,		   // The application's handle
		"DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true),			   // Show extra stats (fps) in title bar?
	camera(0),
	ambientColor(0.1f, 0.15f, 0.25f),
	skyEnabled(true)
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
	for (auto& m : materials) { delete m; }

	delete camera;
	delete sky;
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	LoadAssetsAndCreateEntities();
	
	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the camera
	camera = new Camera(0, 0, -15, 5.0f, 5.0f, XM_PIDIV4, (float)width / height, 0.01f, 100.0f, CameraProjectionType::Perspective);
}


// --------------------------------------------------------
// Loads all necessary assets and creates various entities
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Set up sprite batch and sprite font
	spriteBatch = std::make_unique<SpriteBatch>(context.Get());
	fontArial12     = std::make_unique<SpriteFont>(device.Get(), GetFullPathTo_Wide(L"../../../Assets/Fonts/Arial12.spritefont").c_str());
	fontArial12Bold = std::make_unique<SpriteFont>(device.Get(), GetFullPathTo_Wide(L"../../../Assets/Fonts/Arial12Bold.spritefont").c_str());
	fontArial16	    = std::make_unique<SpriteFont>(device.Get(), GetFullPathTo_Wide(L"../../../Assets/Fonts/Arial16.spritefont").c_str());
	fontArial16Bold	= std::make_unique<SpriteFont>(device.Get(), GetFullPathTo_Wide(L"../../../Assets/Fonts/Arial16Bold.spritefont").c_str());

	// Load 3D models (not using all of them in this demo - could skip some)
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
#define LoadTexture(path, srv) CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(path).c_str(), 0, srv.GetAddressOf());

	LoadTexture(L"../../../Assets/Textures/rock.png", rockSRV);
	LoadTexture(L"../../../Assets/Textures/rock_normals.png", rockNormalsSRV);
	LoadTexture(L"../../../Assets/Textures/cushion.png", cushionSRV);
	LoadTexture(L"../../../Assets/Textures/cushion_normals.png", cushionNormalsSRV);
	LoadTexture(L"../../../Assets/Textures/cobblestone.png", cobblestoneSRV);
	LoadTexture(L"../../../Assets/Textures/cobblestone_normals.png", cobblestoneNormalsSRV);
	LoadTexture(L"../../../Assets/Textures/cobblestone_specular.png", cobblestoneSpecularSRV);


	// Load shaders and create materials
	std::shared_ptr<SimpleVertexShader> basicVertexShader = std::make_shared<SimpleVertexShader>(device, context, GetFullPathTo_Wide(L"VertexShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> basicPixelShader = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(L"PixelShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> normalMapPS = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(L"NormalMapPS.cso").c_str());
	std::shared_ptr<SimplePixelShader> lightAndEnvMapPS = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(L"LightingAndEnvMapPS.cso").c_str());
	std::shared_ptr<SimplePixelShader> envMapOnlyPS = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(L"EnvMapOnlyPS.cso").c_str());
	std::shared_ptr<SimpleVertexShader> skyVS = std::make_shared<SimpleVertexShader>(device, context, GetFullPathTo_Wide(L"SkyVS.cso").c_str());
	std::shared_ptr<SimplePixelShader> skyPS = std::make_shared<SimplePixelShader>(device, context, GetFullPathTo_Wide(L"SkyPS.cso").c_str());

	// Create the sky
	sky = new Sky(
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/right.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/left.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/up.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/down.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/front.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/back.png").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		sampler,
		device,
		context);




	// Create basic materials (no normal maps) ---------------------
	Material* matRock = new Material(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRock->AddSampler("BasicSampler", sampler);
	matRock->AddTextureSRV("SurfaceTexture", rockSRV);
	materials.push_back(matRock); 
	
	Material* matCushion = new Material(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushion->AddSampler("BasicSampler", sampler);
	matCushion->AddTextureSRV("SurfaceTexture", cushionSRV);
	materials.push_back(matCushion);

	Material* matCobblestone = new Material(basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestone->AddSampler("BasicSampler", sampler);
	matCobblestone->AddTextureSRV("SurfaceTexture", cobblestoneSRV);
	matCobblestone->AddTextureSRV("SpecularMap", cobblestoneSpecularSRV);
	materials.push_back(matCobblestone);


	// Create normal mapped materials ---------------------
	Material* matRockNormalMap = new Material(normalMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRockNormalMap->AddSampler("BasicSampler", sampler);
	matRockNormalMap->AddTextureSRV("SurfaceTexture", rockSRV);
	matRockNormalMap->AddTextureSRV("NormalMap", rockNormalsSRV);
	materials.push_back(matRockNormalMap);

	Material* matCushionNormalMap = new Material(normalMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushionNormalMap->AddSampler("BasicSampler", sampler);
	matCushionNormalMap->AddTextureSRV("SurfaceTexture", cushionSRV);
	matCushionNormalMap->AddTextureSRV("NormalMap", cushionNormalsSRV);
	materials.push_back(matCushionNormalMap);

	Material* matCobblestoneNormalMap = new Material(normalMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestoneNormalMap->AddSampler("BasicSampler", sampler);
	matCobblestoneNormalMap->AddTextureSRV("SurfaceTexture", cobblestoneSRV);
	matCobblestoneNormalMap->AddTextureSRV("NormalMap", cobblestoneNormalsSRV);
	matCobblestoneNormalMap->AddTextureSRV("SpecularMap", cobblestoneSpecularSRV);
	materials.push_back(matCobblestoneNormalMap);


	// Create normal mapped & environment mapped materials ---------------------
	Material* matRockLitEnvMap = new Material(lightAndEnvMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRockLitEnvMap->AddSampler("BasicSampler", sampler);
	matRockLitEnvMap->AddTextureSRV("SurfaceTexture", rockSRV);
	matRockLitEnvMap->AddTextureSRV("NormalMap", rockNormalsSRV);
	matRockLitEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());
	materials.push_back(matRockLitEnvMap);

	Material* matCushionLitEnvMap = new Material(lightAndEnvMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushionLitEnvMap->AddSampler("BasicSampler", sampler);
	matCushionLitEnvMap->AddTextureSRV("SurfaceTexture", cushionSRV);
	matCushionLitEnvMap->AddTextureSRV("NormalMap", cushionNormalsSRV);
	matCushionLitEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());
	materials.push_back(matCushionLitEnvMap);

	Material* matCobblestoneLitEnvMap = new Material(lightAndEnvMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestoneLitEnvMap->AddSampler("BasicSampler", sampler);
	matCobblestoneLitEnvMap->AddTextureSRV("SurfaceTexture", cobblestoneSRV);
	matCobblestoneLitEnvMap->AddTextureSRV("NormalMap", cobblestoneNormalsSRV);
	matCobblestoneLitEnvMap->AddTextureSRV("SpecularMap", cobblestoneSpecularSRV);
	matCobblestoneLitEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());
	materials.push_back(matCobblestoneLitEnvMap);

	// Create environment mapped only materials ---------------------
	Material* matRockEnvMap = new Material(envMapOnlyPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRockEnvMap->AddSampler("BasicSampler", sampler);
	matRockEnvMap->AddTextureSRV("NormalMap", rockNormalsSRV);
	matRockEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());
	materials.push_back(matRockEnvMap);

	Material* matCushionEnvMap = new Material(envMapOnlyPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushionEnvMap->AddSampler("BasicSampler", sampler);
	matCushionEnvMap->AddTextureSRV("NormalMap", cushionNormalsSRV);
	matCushionEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());
	materials.push_back(matCushionEnvMap);

	Material* matCobblestoneEnvMap = new Material(envMapOnlyPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestoneEnvMap->AddSampler("BasicSampler", sampler);
	matCobblestoneEnvMap->AddTextureSRV("NormalMap", cobblestoneNormalsSRV);
	matCobblestoneEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());
	materials.push_back(matCobblestoneEnvMap);


	

	// Create three sets of entities - with and without normal maps and env map
	entities.push_back(new GameEntity(cubeMesh, matRock));
	entities.push_back(new GameEntity(sphereMesh, matRock));
	entities.push_back(new GameEntity(cubeMesh, matCushion));
	entities.push_back(new GameEntity(sphereMesh, matCushion));
	entities.push_back(new GameEntity(cubeMesh, matCobblestone));
	entities.push_back(new GameEntity(sphereMesh, matCobblestone));
	entities.push_back(new GameEntity(cubeMesh, matRockNormalMap));
	entities.push_back(new GameEntity(sphereMesh, matRockNormalMap));
	entities.push_back(new GameEntity(cubeMesh, matCushionNormalMap));
	entities.push_back(new GameEntity(sphereMesh, matCushionNormalMap));
	entities.push_back(new GameEntity(cubeMesh, matCobblestoneNormalMap));
	entities.push_back(new GameEntity(sphereMesh, matCobblestoneNormalMap));
	entities.push_back(new GameEntity(cubeMesh, matRockLitEnvMap));
	entities.push_back(new GameEntity(sphereMesh, matRockLitEnvMap));
	entities.push_back(new GameEntity(cubeMesh, matCushionLitEnvMap));
	entities.push_back(new GameEntity(sphereMesh, matCushionLitEnvMap));
	entities.push_back(new GameEntity(cubeMesh, matCobblestoneLitEnvMap));
	entities.push_back(new GameEntity(sphereMesh, matCobblestoneLitEnvMap));
	entities.push_back(new GameEntity(cubeMesh, matRockEnvMap));
	entities.push_back(new GameEntity(sphereMesh, matRockEnvMap));
	entities.push_back(new GameEntity(cubeMesh, matCushionEnvMap));
	entities.push_back(new GameEntity(sphereMesh, matCushionEnvMap));
	entities.push_back(new GameEntity(cubeMesh, matCobblestoneEnvMap));
	entities.push_back(new GameEntity(sphereMesh, matCobblestoneEnvMap));

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

	// Toggle sky
	if (Input::GetInstance().KeyPress('Y'))
		skyEnabled = !skyEnabled;

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
	// Background color (Black in this case) for clearing
	const float color[4] = { 0, 0, 0, 0 };

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
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetFloat("time", totalTime);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());

		// Draw one entity
		e->Draw(context, camera);
	}

	// Draw the sky after all regular entities
	if(skyEnabled)
		sky->Draw(camera);


	// === SpriteBatch =====================================
	// See these links for more info!
	// SpriteBatch: https://github.com/microsoft/DirectXTK/wiki/SpriteBatch
	// SpriteFont: https://github.com/microsoft/DirectXTK/wiki/SpriteFont
	{
		// Make a rectangle for each of the output images
		RECT imageRect     = { 10, 10,     128+10, 128+10 };
		RECT normalMapRect = { 10, 128+20, 128+10, 256+20 };
		RECT fontSheetRect = { 10, 256+30, 128+10, 384+30 };

		// Grab the SRV of the font from the SpriteFont
		// Note: It's not great to do this every frame, but 
		// this is just a demo to show what it looks like!
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fontSheet;
		fontArial16->GetSpriteSheet(fontSheet.GetAddressOf());

		// Begin the batch, draw lots of stuff, then end it
		spriteBatch->Begin();

		// Draw a few 2D textures around the screen
		spriteBatch->Draw(materials[4]->GetTextureSRV("SurfaceTexture").Get(), imageRect);
		spriteBatch->Draw(materials[4]->GetTextureSRV("NormalMap").Get(), normalMapRect);
		spriteBatch->Draw(fontSheet.Get(), fontSheetRect);

		// Draw some arbitrary text
		fontArial16->DrawString(spriteBatch.get(), "Press Y to toggle sky: ", XMFLOAT2(128 + 20, 10));
		fontArial16Bold->DrawString(spriteBatch.get(), skyEnabled ? "Enabled" : "Disabled", XMFLOAT2(360, 10));

		// Draw the mouse position
		int mouseX = Input::GetInstance().GetMouseX();
		int mouseY = Input::GetInstance().GetMouseY();
		std::string dynamicText = "Mouse Pos: {" + std::to_string(mouseX) + ", " + std::to_string(mouseY)+  "}";
		fontArial16->DrawString(spriteBatch.get(), dynamicText.c_str(), XMFLOAT2(128 + 20, 35));

		// Done with the batch
		spriteBatch->End();

		// Reset any states that may be changed by sprite batch!
		context->OMSetBlendState(0, 0, 0xFFFFFFFF);
		context->RSSetState(0);
		context->OMSetDepthStencilState(0, 0);
	}
	// ======================================================


	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);

	// Due to the usage of a more sophisticated swap chain,
	// the render target must be re-bound after every call to Present()
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthStencilView.Get());
}