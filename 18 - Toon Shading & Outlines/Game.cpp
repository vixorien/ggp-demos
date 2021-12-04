#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Assets.h"

#include "WICTextureLoader.h"

#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)


// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// Defining several different methods for toon shading (including none at all)
#define TOON_SHADING_NONE			0
#define TOON_SHADING_RAMP			1
#define TOON_SHADING_CONDITIONALS	2

// Defining several methods for outlines (including none at all)
#define OUTLINE_MODE_NONE				0
#define OUTLINE_MODE_INSIDE_OUT			1
#define OUTLINE_MODE_SOBEL_FILTER		2
#define OUTLINE_MODE_SILHOUETTE			3
#define OUTLINE_MODE_DEPTH_NORMALS		4

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
	ambientColor(0, 0, 0), // Ambient is zero'd out
	freezeLightMovement(false),
	lightCount(3),
	sky(0),
	outlineRenderingMode(OUTLINE_MODE_NONE),
	silhouetteID(0)
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
	// Since we've created these objects within this class (Game),
	// this is also where we should delete them!
	for (auto& m : materials) delete m;
	for (auto& e : entities) delete e;

	delete camera;
	delete sky;

	delete& Assets::GetInstance();
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Seed random
	srand((unsigned int)time(0));

	// Loading scene stuff
	LoadAssetsAndCreateEntities();

	// Set up lights
	lightCount = 1;
	GenerateLights();
	
	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the camera
	camera = new Camera(-0.5f, 6, -15, 5.0f, 5.0f, XM_PIDIV4, (float)width / height, 0.01f, 100.0f, CameraProjectionType::Perspective);
}


// --------------------------------------------------------
// Loads all necessary assets and creates various entities
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Initialize the asset manager and load all assets
	Assets& assets = Assets::GetInstance();
	assets.Initialize("../../../Assets/", device, context, true);
	assets.LoadAllAssets();

	// Set up the initial post process resources
	ResizePostProcessResources();

	// Set up sprite batch and sprite font
	spriteBatch = std::make_unique<SpriteBatch>(context.Get());

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

	// Create a second sampler for with clamp address mode
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	device->CreateSamplerState(&sampDesc, clampSampler.GetAddressOf());

	// Outline rasterizer mode for inside out mesh technique
	D3D11_RASTERIZER_DESC outlineRS = {};
	outlineRS.CullMode = D3D11_CULL_FRONT;
	outlineRS.FillMode = D3D11_FILL_SOLID;
	outlineRS.DepthClipEnable = true;
	device->CreateRasterizerState(&outlineRS, insideOutRasterState.GetAddressOf());


	// Create the sky (loading custom shaders in-line below)
	sky = new Sky(
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/right.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/left.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/up.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/down.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/front.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/back.png").c_str(),
		assets.GetMesh("Models/cube"),
		assets.GetVertexShader("SkyVS"),
		assets.GetPixelShader("SkyPS"),
		sampler,
		device,
		context);

	// Create a few simple textures
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteSRV = assets.CreateSolidColorTexture("Textures/White", 2, 2, XMFLOAT4(1, 1, 1, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> greySRV = assets.CreateSolidColorTexture("Textures/Grey", 2, 2, XMFLOAT4(0.5f, 0.5f, 0.5f, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blackSRV = assets.CreateSolidColorTexture("Textures/Black", 2, 2, XMFLOAT4(0, 0, 0, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> flatNormalsSRV = assets.CreateSolidColorTexture("Textures/FlatNormals", 2, 2, XMFLOAT4(0.5f, 0.5f, 1.0f, 1));

	// Grab shaders needed below
	std::shared_ptr<SimpleVertexShader> vertexShader = assets.GetVertexShader("VertexShader");
	std::shared_ptr<SimplePixelShader> toonPS = assets.GetPixelShader("ToonPS");

	// Create materials
	Material* whiteMat = new Material(toonPS, vertexShader, XMFLOAT3(1, 1, 1));
	whiteMat->AddSampler("BasicSampler", sampler);
	whiteMat->AddSampler("ClampSampler", clampSampler);
	whiteMat->AddTextureSRV("Albedo", whiteSRV);
	whiteMat->AddTextureSRV("NormalMap", flatNormalsSRV);
	whiteMat->AddTextureSRV("RoughnessMap", blackSRV);

	Material* redMat = new Material(toonPS, vertexShader, XMFLOAT3(0.8f, 0, 0));
	redMat->AddSampler("BasicSampler", sampler);
	redMat->AddSampler("ClampSampler", clampSampler);
	redMat->AddTextureSRV("Albedo", whiteSRV);
	redMat->AddTextureSRV("NormalMap", flatNormalsSRV);
	redMat->AddTextureSRV("RoughnessMap", blackSRV);

	Material* detailedMat = new Material(toonPS, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	detailedMat->AddSampler("BasicSampler", sampler);
	detailedMat->AddSampler("ClampSampler", clampSampler);
	detailedMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/cushion"));
	detailedMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/cushion_normals"));
	detailedMat->AddTextureSRV("RoughnessMap", blackSRV);

	Material* crateMat = new Material(toonPS, vertexShader, XMFLOAT3(1, 1, 1));
	crateMat->AddSampler("BasicSampler", sampler);
	crateMat->AddSampler("ClampSampler", clampSampler);
	crateMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/crate_wood_albedo"));
	crateMat->AddTextureSRV("NormalMap", flatNormalsSRV);
	crateMat->AddTextureSRV("RoughnessMap", greySRV);

	Material* mandoMat = new Material(toonPS, vertexShader, XMFLOAT3(1, 1, 1));
	mandoMat->AddSampler("BasicSampler", sampler);
	mandoMat->AddSampler("ClampSampler", clampSampler);
	mandoMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/mando"));
	mandoMat->AddTextureSRV("NormalMap", flatNormalsSRV);
	mandoMat->AddTextureSRV("RoughnessMap", blackSRV);

	Material* containerMat = new Material(toonPS, vertexShader, XMFLOAT3(1, 1, 1));
	containerMat->AddSampler("BasicSampler", sampler);
	containerMat->AddSampler("ClampSampler", clampSampler);
	containerMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/container"));
	containerMat->AddTextureSRV("NormalMap", flatNormalsSRV);
	containerMat->AddTextureSRV("RoughnessMap", greySRV);


	materials.push_back(whiteMat);
	materials.push_back(redMat);
	materials.push_back(crateMat);
	materials.push_back(detailedMat);
	materials.push_back(mandoMat);
	materials.push_back(containerMat);


	// Grab meshes
	Mesh* sphereMesh = assets.GetMesh("Models/sphere");
	Mesh* torusMesh = assets.GetMesh("Models/torus");
	Mesh* crateMesh = assets.GetMesh("Models/crate_wood");
	Mesh* mandoMesh = assets.GetMesh("Models/mando");
	Mesh* containerMesh = assets.GetMesh("Models/container");

	// === Create the line up entities =====================================
	GameEntity* sphere = new GameEntity(sphereMesh, whiteMat);
	sphere->GetTransform()->SetPosition(0, 0, 0);

	GameEntity* torus = new GameEntity(torusMesh, redMat);
	torus->GetTransform()->SetScale(2.0f);
	torus->GetTransform()->SetRotation(0, 0, XM_PIDIV2);
	torus->GetTransform()->SetPosition(0, -3, 0);

	GameEntity* detailed = new GameEntity(sphereMesh, detailedMat);
	detailed->GetTransform()->SetPosition(0, -6, 0);

	GameEntity* mando = new GameEntity(mandoMesh, mandoMat);
	mando->GetTransform()->SetPosition(0, -9, 0);

	GameEntity* crate = new GameEntity(crateMesh, crateMat);
	crate->GetTransform()->SetPosition(0, -12, 0);

	GameEntity* container = new GameEntity(containerMesh, containerMat);
	container->GetTransform()->SetPosition(0, -16, 0);
	container->GetTransform()->SetScale(0.075f);


	entities.push_back(sphere);
	entities.push_back(torus);
	entities.push_back(detailed);
	entities.push_back(mando);
	entities.push_back(crate);
	entities.push_back(container);



}


// ------------------------------------------------------------
// Resizes (by releasing and re-creating) the resources
// required for post processing.  Note the useage of 
// ComPtr's .ReleaseAndGetAddressOf() method for this. 
// 
// We only need to do this at start-up and whenever the 
// window is resized.
// ------------------------------------------------------------
void Game::ResizePostProcessResources()
{
	// Reset all resources (releasing them)
	ppRTV.Reset();
	ppSRV.Reset();
	sceneNormalsRTV.Reset();
	sceneNormalsSRV.Reset();
	sceneDepthRTV.Reset();
	sceneDepthSRV.Reset();

	// Describe our textures
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.ArraySize = 1;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Will render to it and sample from it!
	textureDesc.CPUAccessFlags = 0;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.MipLevels = 1;
	textureDesc.MiscFlags = 0;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;

	// Create the color and normals textures
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ppTexture;
	device->CreateTexture2D(&textureDesc, 0, ppTexture.GetAddressOf());

	// Adjust the description for scene normals
	textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneNormalsTexture;
	device->CreateTexture2D(&textureDesc, 0, sceneNormalsTexture.GetAddressOf());

	// Adjust the description for the scene depths
	textureDesc.Format = DXGI_FORMAT_R32_FLOAT;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneDepthsTexture;
	device->CreateTexture2D(&textureDesc, 0, sceneDepthsTexture.GetAddressOf());

	// Create the Render Target Views (null descriptions use default settings)
	device->CreateRenderTargetView(ppTexture.Get(), 0, ppRTV.GetAddressOf());
	device->CreateRenderTargetView(sceneNormalsTexture.Get(), 0, sceneNormalsRTV.GetAddressOf());
	device->CreateRenderTargetView(sceneDepthsTexture.Get(), 0, sceneDepthRTV.GetAddressOf());

	// Create the Shader Resource Views (null descriptions use default settings)
	device->CreateShaderResourceView(ppTexture.Get(), 0, ppSRV.GetAddressOf());
	device->CreateShaderResourceView(sceneNormalsTexture.Get(), 0, sceneNormalsSRV.GetAddressOf());
	device->CreateShaderResourceView(sceneDepthsTexture.Get(), 0, sceneDepthSRV.GetAddressOf());
}


void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = 1.0f;

	Light dir2 = {};
	dir2.Type = LIGHT_TYPE_DIRECTIONAL;
	dir2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dir2.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir2.Intensity = 1.0f;

	Light dir3 = {};
	dir3.Type = LIGHT_TYPE_DIRECTIONAL;
	dir3.Direction = XMFLOAT3(0, -1, 1);
	dir3.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir3.Intensity = 1.0f;

	// Add light to the list
	lights.push_back(dir1);
	lights.push_back(dir2);
	lights.push_back(dir3);

	// Create the rest of the lights
	while (lights.size() < MAX_LIGHTS)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-15.0f, 15.0f), RandomRange(-2.0f, 5.0f), RandomRange(-15.0f, 15.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 3.0f);

		// Add to the list
		lights.push_back(point);
	}

	// Make sure we're exactly MAX_LIGHTS big
	lights.resize(MAX_LIGHTS);
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

	// Reset post process stuff because to match window
	ResizePostProcessResources();
}


// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// In the event we need it below
	Assets& assets = Assets::GetInstance();

	// Example input checking: Quit if the escape key is pressed
	Input& input = Input::GetInstance();
	if (input.KeyDown(VK_ESCAPE))
		Quit();

	// Update the camera this frame
	camera->Update(deltaTime);

	// Check individual input
	if (input.KeyPress(VK_TAB)) { 
		outlineRenderingMode++; 
		if(outlineRenderingMode > OUTLINE_MODE_DEPTH_NORMALS) outlineRenderingMode = OUTLINE_MODE_NONE; 
	}
	if (input.KeyPress('F')) freezeLightMovement = !freezeLightMovement;
	

	// Handle light count changes, clamped appropriately
	if (input.KeyDown(VK_UP)) lightCount++;
	if (input.KeyDown(VK_DOWN)) lightCount--;
	lightCount = max(1, min(MAX_LIGHTS, lightCount));

	// Move lights
	for (int i = 0; i < lightCount && !freezeLightMovement; i++)
	{
		// Only adjust point lights
		if (lights[i].Type == LIGHT_TYPE_POINT)
		{
			// Adjust either X or Z
			float lightAdjust = sin(totalTime + i) * 5;

			if (i % 2 == 0) lights[i].Position.x = lightAdjust;
			else			lights[i].Position.z = lightAdjust;
		}
	}

	// Slowly rotate entities
	for (auto& e : entities)
	{
		e->GetTransform()->Rotate(0, deltaTime * 0.1f, 0);
	}
}



// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Any PRE-RENDER steps we need to take care of?
	// - Clearing the render target and depth buffer
	// - Usually post-processing related things, too
	PreRender();

	// Reset the silhouette ID before rendering any entities
	silhouetteID = 0;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
		toonRamp1 = Assets::GetInstance().GetTexture("Textures/Ramps/toonRamp1"),
		toonRamp2 = Assets::GetInstance().GetTexture("Textures/Ramps/toonRamp2"),
		toonRamp3 = Assets::GetInstance().GetTexture("Textures/Ramps/toonRamp3"),
		toonRampSpec = Assets::GetInstance().GetTexture("Textures/Ramps/toonRampSpecular");

	// Render entities with several different toon shading variations
	RenderEntitiesWithToonShading(TOON_SHADING_NONE, 0, true, XMFLOAT3(-6, 7.5f, 0));
	RenderEntitiesWithToonShading(TOON_SHADING_CONDITIONALS, 0, true, XMFLOAT3(-3, 7.5f, 0));
	RenderEntitiesWithToonShading(TOON_SHADING_RAMP, toonRamp1, true, XMFLOAT3(0, 7.5f,0));
	RenderEntitiesWithToonShading(TOON_SHADING_RAMP, toonRamp2, true, XMFLOAT3(3, 7.5f, 0));
	RenderEntitiesWithToonShading(TOON_SHADING_RAMP, toonRamp3, true, XMFLOAT3(6, 7.5f, 0));

	// Draw the sky after all regular entities
	sky->Draw(camera);

	// Draw labels in 3D space
	DrawTextAtLocation("Standard shading", XMFLOAT3(-7, 9.0f, 0), XMFLOAT2(0.2f, 0.2f));
	DrawTextAtLocation("Toon shading\nwith conditionals\nin the shader", XMFLOAT3(-4, 9.5f, 0), XMFLOAT2(0.2f, 0.2f));
	DrawTextAtLocation("Toon shading using\nabove ramp texture\nw/ black left-most pixel", XMFLOAT3(-1, 9.5f, 0), XMFLOAT2(0.2f, 0.2f));
	DrawTextAtLocation("Toon shading using\nabove ramp texture\nwith 3 total bands", XMFLOAT3(2, 9.5f, 0), XMFLOAT2(0.2f, 0.2f));
	DrawTextAtLocation("Toon shading using\nabove ramp texture\nwith 2 total bands", XMFLOAT3(5, 9.5f, 0), XMFLOAT2(0.2f, 0.2f));
	DrawTextAtLocation("All three ramp materials\nare using this texture\nas their specular ramp", XMFLOAT3(8, 9.5f, 0), XMFLOAT2(0.2f, 0.2f));
	
	// Draw sprites to show ramp textures
	DrawSpriteAtLocation(toonRamp1, XMFLOAT3(0, 11, 0), XMFLOAT2(2, 2));
	DrawSpriteAtLocation(toonRamp2, XMFLOAT3(3, 11, 0), XMFLOAT2(2, 2));
	DrawSpriteAtLocation(toonRamp3, XMFLOAT3(6, 11, 0), XMFLOAT2(2, 2));

	// Show specular ramp, too
	DrawSpriteAtLocation(toonRampSpec, XMFLOAT3(9, 11, 0), XMFLOAT2(2, 2));

	// Post-scene-render things now
	// - Usually post processing
	PostRender();

	// Draw the UI on top of everything
	DrawUI();

	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);

	// Due to the usage of a more sophisticated swap chain,
	// the render target must be re-bound after every call to Present()
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthStencilView.Get());
}



// --------------------------------------------------------
// Clears buffers and sets up render targets
// --------------------------------------------------------
void Game::PreRender()
{
	// Background color for clearing
	const float color[4] = { 0, 0, 0, 1 };

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Clear all render targets, too
	context->ClearRenderTargetView(ppRTV.Get(), color);
	context->ClearRenderTargetView(sceneNormalsRTV.Get(), color);
	context->ClearRenderTargetView(sceneDepthRTV.Get(), color);

	// Assume three render targets (since pixel shader is always returning 3 numbers)
	ID3D11RenderTargetView* rtvs[3] =
	{
		backBufferRTV.Get(),
		sceneNormalsRTV.Get(),
		sceneDepthRTV.Get()
	};

	// Swap to the post process target if we need it
	if (outlineRenderingMode != OUTLINE_MODE_NONE && outlineRenderingMode != OUTLINE_MODE_INSIDE_OUT)
		rtvs[0] = ppRTV.Get();

	// Set all three
	context->OMSetRenderTargets(3, rtvs, depthStencilView.Get());
}

// --------------------------------------------------------
// Applies post processing if necessary
// --------------------------------------------------------
void Game::PostRender()
{
	Assets& assets = Assets::GetInstance();


	std::shared_ptr<SimpleVertexShader> fullscreenVS = assets.GetVertexShader("FullscreenTriangleVS");
	std::shared_ptr<SimplePixelShader> sobelFilterPS = assets.GetPixelShader("SobelFilterPS");
	std::shared_ptr<SimplePixelShader> silhouettePS = assets.GetPixelShader("SilhouettePS");
	std::shared_ptr<SimplePixelShader> depthNormalOutlinePS = assets.GetPixelShader("DepthNormalOutlinePS");

	// Which form of outline are we handling?
	switch (outlineRenderingMode)
	{

	case OUTLINE_MODE_SOBEL_FILTER:
		// Now that the scene is rendered, swap to the back buffer
		context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), 0);

		// Set up post process shaders
		fullscreenVS->SetShader();

		// Note: Probably needs a clamp sampler, too
		sobelFilterPS->SetShader();
		sobelFilterPS->SetShaderResourceView("pixels", ppSRV.Get());
		sobelFilterPS->SetSamplerState("samplerOptions", clampSampler.Get());
		sobelFilterPS->SetFloat("pixelWidth", 1.0f / width);
		sobelFilterPS->SetFloat("pixelHeight", 1.0f / height);
		sobelFilterPS->CopyAllBufferData();

		// Draw exactly 3 vertices, which the special post-process vertex shader will
		// "figure out" on the fly (resulting in our "full screen triangle")
		context->Draw(3, 0);

		break;


	case OUTLINE_MODE_SILHOUETTE:

		// Now that the scene is rendered, swap to the back buffer
		context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), 0);

		// Set up post process shaders
		assets.GetVertexShader("FullscreenTriangleVS")->SetShader();

		silhouettePS->SetShaderResourceView("pixels", ppSRV.Get());
		silhouettePS->SetSamplerState("samplerOptions", clampSampler.Get());
		silhouettePS->SetShader();

		silhouettePS->SetFloat("pixelWidth", 1.0f / width);
		silhouettePS->SetFloat("pixelHeight", 1.0f / height);
		silhouettePS->CopyAllBufferData();

		// Draw exactly 3 vertices, which the special post-process vertex shader will
		// "figure out" on the fly (resulting in our "full screen triangle")
		context->Draw(3, 0);

		break;

	case OUTLINE_MODE_DEPTH_NORMALS:

		// Now that the scene is rendered, swap to the back buffer
		context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), 0);

		// Set up post process shaders
		assets.GetVertexShader("FullscreenTriangleVS")->SetShader();

		depthNormalOutlinePS->SetShaderResourceView("pixels", ppSRV.Get());
		depthNormalOutlinePS->SetShaderResourceView("normals", sceneNormalsSRV.Get());
		depthNormalOutlinePS->SetShaderResourceView("depth", sceneDepthSRV.Get());
		depthNormalOutlinePS->SetSamplerState("samplerOptions", clampSampler.Get());
		depthNormalOutlinePS->SetShader();

		depthNormalOutlinePS->SetFloat("pixelWidth", 1.0f / width);
		depthNormalOutlinePS->SetFloat("pixelHeight", 1.0f / height);
		depthNormalOutlinePS->SetFloat("depthAdjust", 5.0f);
		depthNormalOutlinePS->SetFloat("normalAdjust", 5.0f);
		depthNormalOutlinePS->CopyAllBufferData();

		// Draw exactly 3 vertices, which the special post-process vertex shader will
		// "figure out" on the fly (resulting in our "full screen triangle")
		context->Draw(3, 0);

		break;
	}


	// Unbind shader resource views at the end of the frame,
	// since we'll be rendering into one of those textures
	// at the start of the next
	ID3D11ShaderResourceView* nullSRVs[128] = {};
	context->PSSetShaderResources(0, ARRAYSIZE(nullSRVs), nullSRVs);
}


// --------------------------------------------------------
// Draws basic text info for the user
// --------------------------------------------------------
void Game::DrawUI()
{
	// Grab the font from the asset manager
	Assets& assets = Assets::GetInstance();
	std::shared_ptr<SpriteFont> fontArial12 = assets.GetSpriteFont("Fonts/Arial12");

	spriteBatch->Begin();

	// Title
	fontArial12->DrawString(spriteBatch.get(), "Toon Shading & Outline Demo", XMFLOAT2(10, 10), Colors::Black);

	// Description
	fontArial12->DrawString(spriteBatch.get(), "This demo shows several\nTOON (cel) shading and\nOUTLINE techniques.", XMFLOAT2(10, 40), Colors::Black);

	// Controls
	fontArial12->DrawString(spriteBatch.get(), "== Controls ==", XMFLOAT2(10, 130), Colors::Black);
	fontArial12->DrawString(spriteBatch.get(), "Tab: Change outline mode", XMFLOAT2(10, 150), Colors::Black);
	fontArial12->DrawString(spriteBatch.get(), "Up/Down: Adjust active lights", XMFLOAT2(10, 170), Colors::Black);

	// Info on current outline mode
	fontArial12->DrawString(spriteBatch.get(), "== OUTLINE MODE ==", XMFLOAT2(10, 220), Colors::Black);
	fontArial12->DrawString(spriteBatch.get(), "Current Outline:", XMFLOAT2(10, 240), Colors::Black);
	switch (outlineRenderingMode)
	{
	case OUTLINE_MODE_NONE:
		fontArial12->DrawString(spriteBatch.get(), "NONE", XMFLOAT2(120, 240), Colors::DarkRed);
		break;

	case OUTLINE_MODE_INSIDE_OUT:
		fontArial12->DrawString(spriteBatch.get(), "Inside Out Mesh", XMFLOAT2(120, 240), Colors::Green);
		fontArial12->DrawString(spriteBatch.get(), "This mode literally draws each object\ninside out, using a special vertex\nshader that moves the vertices along\ntheir normals.  This works best when\nthe model has no hard edges.", XMFLOAT2(10, 270), Colors::Black);
		fontArial12->DrawString(spriteBatch.get(), "As you can see, the sphere and torus\nwork the best here, as they have no\nhard edges. Outlines on the helmet and\ncrate break down with this technique\ndue to the hard edges.", XMFLOAT2(10, 370), Colors::Black);
		break;

	case OUTLINE_MODE_SOBEL_FILTER:
		fontArial12->DrawString(spriteBatch.get(), "Sobel Filter Post Process", XMFLOAT2(120, 240), Colors::Green);
		fontArial12->DrawString(spriteBatch.get(), "This mode uses a simple post process\nto compare surrounding pixel colors\nand, based on the strength of color\ndifferences, interpolates towards an\noutline color.", XMFLOAT2(10, 270), Colors::Black);
		fontArial12->DrawString(spriteBatch.get(), "This is easy to implement but clearly\ngets a bit noisy, as it is completely\nbased on pixel colors.  This works \nbest on areas of flat color, like the\nvery simple toon shading examples.\nThis technique is the basis of many\nPhotoshop filters.", XMFLOAT2(10, 370), Colors::Black);
		break;

	case OUTLINE_MODE_SILHOUETTE:
		fontArial12->DrawString(spriteBatch.get(), "Silhouette Post Process", XMFLOAT2(120, 240), Colors::Green);
		fontArial12->DrawString(spriteBatch.get(), "This mode outputs a unique ID value to\nthe alpha channel of the main render\ntarget.  A post process then changes\nthe current pixel to black when a\nneighboring pixel has a different ID value.", XMFLOAT2(10, 270), Colors::Black);
		fontArial12->DrawString(spriteBatch.get(), "This technique only puts outlines around\nthe silhouette of the object. There are no\n'interior' edges being outlined.  This may\nor may not be the desired effect!", XMFLOAT2(10, 370), Colors::Black);

		break;

	case OUTLINE_MODE_DEPTH_NORMALS:
		fontArial12->DrawString(spriteBatch.get(), "Normal & Depth Post Process", XMFLOAT2(120, 240), Colors::Green);
		fontArial12->DrawString(spriteBatch.get(), "This mode uses multiple active render\ntargets to capture not only the colors\nof the scene, but the normals and depths,\ntoo.  A post process then compares\nneighboring normals & depths.", XMFLOAT2(10, 270), Colors::Black);
		fontArial12->DrawString(spriteBatch.get(), "The post process used by this technique\nworks similarly to the Sobel filter, except\nit compares normals of surrounding pixels\nas well as the depths of surrounding pixels.", XMFLOAT2(10, 370), Colors::Black);
		fontArial12->DrawString(spriteBatch.get(), "A large enough discrepancy in either the\nnormals or the depths of surrounding pixels\ncauses an outline to appear.", XMFLOAT2(10, 430), Colors::Black);

		break;
	}



	spriteBatch->End();

	// Reset render states altered by sprite batch!
	context->RSSetState(0);
	context->OMSetDepthStencilState(0, 0);
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);

}

// --------------------------------------------------------
// Draws the given sprite (texture) at the specified location in 3D space
// --------------------------------------------------------
void Game::DrawSpriteAtLocation(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv, XMFLOAT3 position, XMFLOAT2 scale, XMFLOAT3 pitchYawRoll)
{
	XMFLOAT4X4 view = camera->GetView();
	XMFLOAT4X4 proj = camera->GetProjection();

	// Create our own world view projection matrix to use 
	// with sprite batch
	XMMATRIX wvp =
		XMMatrixScaling(0.5f * scale.x, -0.5f * scale.y, 1) *
		XMMatrixRotationRollPitchYaw(pitchYawRoll.x, pitchYawRoll.y, pitchYawRoll.z) *
		XMMatrixTranslation(position.x, position.y, position.z) *
		XMLoadFloat4x4(&view) *
		XMLoadFloat4x4(&proj);

	// Set the rotation mode to unspecified, which has the side effect of letting us
	// use our own custom transform matrix without any alterations
	spriteBatch->SetRotation(DXGI_MODE_ROTATION_UNSPECIFIED);

	// Begin the batch in Immediate mode, which will set render states
	// right away (allowing us to change them before drawing) and passing
	// in our custom world view projection
	spriteBatch->Begin(SpriteSortMode_Immediate, 0, 0, 0, 0, 0, wvp);

	// Reset the depth state to respect depth!
	context->OMSetDepthStencilState(0, 0);

	// Basic rectangle - we'll be moving the sprite with the matrix above
	RECT r = { -1, -1, 1, 1 };
	spriteBatch->Draw(srv.Get(), r);

	// All done
	spriteBatch->End();

	// Reset all states just in case
	context->RSSetState(0);
	context->OMSetDepthStencilState(0, 0);
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);

	// Reset this, too
	spriteBatch->SetRotation(DXGI_MODE_ROTATION_IDENTITY);
}



// --------------------------------------------------------
// Draws the given text at the specified location in 3D space
// --------------------------------------------------------
void Game::DrawTextAtLocation(const char* text, DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 scale, DirectX::XMFLOAT3 pitchYawRoll)
{
	// Assuming it's a 72 point font!
	float height = 72;

	XMFLOAT4X4 view = camera->GetView();
	XMFLOAT4X4 proj = camera->GetProjection();

	// Create our own world view projection matrix to use 
	// with sprite batch
	XMMATRIX wvp =
		XMMatrixScaling(scale.x / height, -scale.y / height, 1) *
		XMMatrixRotationRollPitchYaw(pitchYawRoll.x, pitchYawRoll.y, pitchYawRoll.z) *
		XMMatrixTranslation(position.x, position.y, position.z) *
		XMLoadFloat4x4(&view) *
		XMLoadFloat4x4(&proj);

	// Set the rotation mode to unspecified, which has the side effect of letting us
	// use our own custom transform matrix without any alterations
	spriteBatch->SetRotation(DXGI_MODE_ROTATION_UNSPECIFIED);

	// Begin the batch in Immediate mode, which will set render states
	// right away (allowing us to change them before drawing) and passing
	// in our custom world view projection
	spriteBatch->Begin(SpriteSortMode_Immediate, 0, 0, 0, 0, 0, wvp);

	// Reset the depth state to respect depth!
	context->OMSetDepthStencilState(0, 0);

	// Use the sprite font to draw the specified text - we'll be
	// moving the text with the matrix above
	Assets::GetInstance().GetSpriteFont("Fonts/Arial72")->DrawString(
		spriteBatch.get(),
		text,
		XMFLOAT2(0, 0),
		Colors::Black);

	// All done
	spriteBatch->End();

	// Reset all states just in case
	context->RSSetState(0);
	context->OMSetDepthStencilState(0, 0);
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);

	// Reset this, too
	spriteBatch->SetRotation(DXGI_MODE_ROTATION_IDENTITY);
}


// --------------------------------------------------------
// Renders entities, potentially with toon shading and 
// an offset to all of their positions
// --------------------------------------------------------
void Game::RenderEntitiesWithToonShading(int toonShadingType, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> toonRamp, bool offsetPositions, DirectX::XMFLOAT3 offset)
{
	// Grab specular ramp just in case
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> toonRampSpecular = Assets::GetInstance().GetTexture("Textures/Ramps/toonRampSpecular");

	// Loop through the game entities in the current scene and draw
	for (auto& e : entities)
	{
		// Set total time on this entity's material's pixel shader
		// Note: If the shader doesn't have this variable, nothing happens
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("lightCount", lightCount);
		ps->SetInt("toonShadingType", toonShadingType);

		// Need to set the silhouette ID if that's the outline mode
		if (outlineRenderingMode == OUTLINE_MODE_SILHOUETTE)
		{
			ps->SetInt("silhouetteID", silhouetteID);
			silhouetteID++; // Increment, too!
		}

		// Set toon-shading textures if necessary
		if (toonShadingType == TOON_SHADING_RAMP)
		{
			ps->SetShaderResourceView("ToonRamp", toonRamp);
			ps->SetShaderResourceView("ToonRampSpecular", toonRampSpecular);
		}

		// If we're overriding the position, save the old one
		XMFLOAT3 originalPos = e->GetTransform()->GetPosition();
		if (offsetPositions) e->GetTransform()->MoveAbsolute(offset);

		// Draw one entity
		e->Draw(context, camera);

		// Outline too?
		if (outlineRenderingMode == OUTLINE_MODE_INSIDE_OUT)
			DrawOutlineInsideOut(e, camera, 0.03f);

		// Replace the old position if necessary
		if (offsetPositions) e->GetTransform()->SetPosition(originalPos);
	}

}



// --------------------------------------------------------
// Renders a single entity inside out, using a vertex shader
// that moves each vertex along its normal
// --------------------------------------------------------
void Game::DrawOutlineInsideOut(GameEntity* entity, Camera* camera, float outlineSize)
{
	std::shared_ptr<SimpleVertexShader> insideOutVS = Assets::GetInstance().GetVertexShader("InsideOutVS");
	std::shared_ptr<SimplePixelShader> solidColorPS = Assets::GetInstance().GetPixelShader("SolidColorPS");

	insideOutVS->SetShader();
	solidColorPS->SetShader();

	insideOutVS->SetMatrix4x4("world", entity->GetTransform()->GetWorldMatrix());
	insideOutVS->SetMatrix4x4("view", camera->GetView());
	insideOutVS->SetMatrix4x4("projection", camera->GetProjection());
	insideOutVS->SetFloat("outlineSize", outlineSize);
	insideOutVS->CopyAllBufferData();

	solidColorPS->SetFloat3("Color", XMFLOAT3(0, 0, 0));
	solidColorPS->CopyAllBufferData();

	// Set render states
	context->RSSetState(insideOutRasterState.Get());

	// Draw the mesh
	entity->GetMesh()->SetBuffersAndDraw(context.Get());

	// Reset render states
	context->RSSetState(0);
}
