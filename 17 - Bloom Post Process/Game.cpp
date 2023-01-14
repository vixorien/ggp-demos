#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Assets.h"
#include "Helpers.h"

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
	ambientColor(0, 0, 0), // Ambient is zero'd out since it's not physically-based
	gammaCorrection(false),
	useAlbedoTexture(false),
	useMetalMap(false),
	useNormalMap(false),
	useRoughnessMap(false),
	usePBR(false),
	drawLights(true),
	currentScene(0),
	freezeLightMovement(false),
	lightCount(20),
	bloomLevels(5),
	bloomThreshold(1.0f),
	bloomLevelIntensities{ 1,1,1,1,1 },
	drawBloomTextures(false)
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

	// Deleting the singleton reference we've set up here
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

	// Set the current scene (which of the 3 lists of entities are we drawing)
	currentScene = &entitiesLineup;

	// Loading scene stuff
	LoadAssetsAndCreateEntities();

	// Set up lights
	GenerateLights();
	
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

	// Bloom setup
	{
		// Create post process resources
		ResizeAllPostProcessResources();

		// Sampler state for post processing
		D3D11_SAMPLER_DESC ppSampDesc = {};
		ppSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		ppSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		ppSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		ppSampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		ppSampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		device->CreateSamplerState(&ppSampDesc, ppSampler.GetAddressOf());
	}
}


// --------------------------------------------------------
// Loads all necessary assets and creates various entities
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Initialize the asset manager and set it up to load assets on demand
	Assets& assets = Assets::GetInstance();
	assets.Initialize(L"../../../Assets/", L"./", device, context, true, true);

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


	// Create the sky (loading custom shaders in-line below)
	sky = std::make_shared<Sky>(
		FixPath(L"../../../Assets/Skies/Night Moon/right.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/left.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/up.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/down.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/front.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/back.png").c_str(),
		assets.GetMesh(L"Models/cube"),
		assets.GetVertexShader(L"SkyVS"),
		assets.GetPixelShader(L"SkyPS"),
		sampler,
		device,
		context);


	// Grab shaders needed below
	std::shared_ptr<SimpleVertexShader> vertexShader = assets.GetVertexShader(L"VertexShader");
	std::shared_ptr<SimplePixelShader> pixelShader = assets.GetPixelShader(L"PixelShader");

	// Create basic materials
	std::shared_ptr<Material> cobbleMat2x = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	cobbleMat2x->AddSampler("BasicSampler", sampler);
	cobbleMat2x->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/cobblestone_albedo"));
	cobbleMat2x->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/cobblestone_normals"));
	cobbleMat2x->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/cobblestone_roughness"));
	cobbleMat2x->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/cobblestone_metal"));

	std::shared_ptr<Material> cobbleMat4x = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4x->AddSampler("BasicSampler", sampler);
	cobbleMat4x->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/cobblestone_albedo"));
	cobbleMat4x->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/cobblestone_normals"));
	cobbleMat4x->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/cobblestone_roughness"));
	cobbleMat4x->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/cobblestone_metal"));

	std::shared_ptr<Material> floorMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	floorMat->AddSampler("BasicSampler", sampler);
	floorMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/floor_albedo"));
	floorMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/floor_normals"));
	floorMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/floor_roughness"));
	floorMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/floor_metal"));

	std::shared_ptr<Material> paintMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	paintMat->AddSampler("BasicSampler", sampler);
	paintMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/paint_albedo"));
	paintMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/paint_normals"));
	paintMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/paint_roughness"));
	paintMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/paint_metal"));

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	scratchedMat->AddSampler("BasicSampler", sampler);
	scratchedMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/scratched_albedo"));
	scratchedMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/scratched_normals"));
	scratchedMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/scratched_roughness"));
	scratchedMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/scratched_metal"));

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	bronzeMat->AddSampler("BasicSampler", sampler);
	bronzeMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/bronze_albedo"));
	bronzeMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/bronze_normals"));
	bronzeMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/bronze_roughness"));
	bronzeMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/bronze_metal"));

	std::shared_ptr<Material> roughMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	roughMat->AddSampler("BasicSampler", sampler);
	roughMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/rough_albedo"));
	roughMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/rough_normals"));
	roughMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/rough_roughness"));
	roughMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/rough_metal"));

	std::shared_ptr<Material> woodMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	woodMat->AddSampler("BasicSampler", sampler);
	woodMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/wood_albedo"));
	woodMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/wood_normals"));
	woodMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/wood_roughness"));
	woodMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/wood_metal"));

	// Get meshes needed below
	std::shared_ptr<Mesh> cubeMesh = assets.GetMesh(L"Models/cube");
	std::shared_ptr<Mesh> sphereMesh = assets.GetMesh(L"Models/sphere");


	// === Create the "randomized" entities, with a static floor ===========
	std::shared_ptr<GameEntity> floor = std::make_shared<GameEntity>(cubeMesh, cobbleMat4x);
	floor->GetTransform()->SetScale(50, 50, 50);
	floor->GetTransform()->SetPosition(0, -27, 0);
	entitiesRandom.push_back(floor);

	for (int i = 0; i < 32; i++)
	{
		std::shared_ptr<Material> whichMat = floorMat;
		switch (i % 7)
		{
		case 0: whichMat = floorMat; break;
		case 1: whichMat = paintMat; break;
		case 2: whichMat = cobbleMat2x; break;
		case 3: whichMat = scratchedMat; break;
		case 4: whichMat = bronzeMat; break;
		case 5: whichMat = roughMat; break;
		case 6: whichMat = woodMat; break;
		}

		float size = RandomRange(0.05f, 2.0f);

		std::shared_ptr<GameEntity> sphere = std::make_shared<GameEntity>(sphereMesh, whichMat);
		sphere->GetTransform()->SetScale(size, size, size);
		sphere->GetTransform()->SetPosition(
			RandomRange(-25.0f, 25.0f),
			RandomRange(0.0f, 3.0f),
			RandomRange(-25.0f, 25.0f));

		entitiesRandom.push_back(sphere);
	}



	// === Create the line up entities =====================================
	std::shared_ptr<GameEntity> cobSphere = std::make_shared<GameEntity>(sphereMesh, cobbleMat2x);
	cobSphere->GetTransform()->SetPosition(-6, 0, 0);

	std::shared_ptr<GameEntity> floorSphere = std::make_shared<GameEntity>(sphereMesh, floorMat);
	floorSphere->GetTransform()->SetPosition(-4, 0, 0);

	std::shared_ptr<GameEntity> paintSphere = std::make_shared<GameEntity>(sphereMesh, paintMat);
	paintSphere->GetTransform()->SetPosition(-2, 0, 0);

	std::shared_ptr<GameEntity> scratchSphere = std::make_shared<GameEntity>(sphereMesh, scratchedMat);
	scratchSphere->GetTransform()->SetPosition(0, 0, 0);

	std::shared_ptr<GameEntity> bronzeSphere = std::make_shared<GameEntity>(sphereMesh, bronzeMat);
	bronzeSphere->GetTransform()->SetPosition(2, 0, 0);

	std::shared_ptr<GameEntity> roughSphere = std::make_shared<GameEntity>(sphereMesh, roughMat);
	roughSphere->GetTransform()->SetPosition(4, 0, 0);

	std::shared_ptr<GameEntity> woodSphere = std::make_shared<GameEntity>(sphereMesh, woodMat);
	woodSphere->GetTransform()->SetPosition(6, 0, 0);

	entitiesLineup.push_back(cobSphere);
	entitiesLineup.push_back(floorSphere);
	entitiesLineup.push_back(paintSphere);
	entitiesLineup.push_back(scratchSphere);
	entitiesLineup.push_back(bronzeSphere);
	entitiesLineup.push_back(roughSphere);
	entitiesLineup.push_back(woodSphere);



	// === Create a gradient of entities based on roughness & metalness ====
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteAlbedoSRV = assets.CreateSolidColorTexture(L"Textures/WhiteAlbedo", 2, 2, XMFLOAT4(1, 1, 1, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metal0SRV = assets.CreateSolidColorTexture(L"Textures/Metal0", 2, 2, XMFLOAT4(0, 0, 0, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metal1SRV = assets.CreateSolidColorTexture(L"Textures/Metal1", 2, 2, XMFLOAT4(1, 1, 1, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> flatNormalsSRV = assets.CreateSolidColorTexture(L"Textures/FlatNormals", 2, 2, XMFLOAT4(0.5f, 0.5f, 1.0f, 1));

	for (int i = 0; i <= 10; i++)
	{
		// Roughness value for this entity
		float r = i / 10.0f;

		// Create textures
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughSRV = assets.CreateSolidColorTexture(L"Textures/Rough" + std::to_wstring(r), 2, 2, XMFLOAT4(r, r, r, 1));

		// Set up the materials
		std::shared_ptr<Material> matMetal = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1));
		matMetal->AddSampler("BasicSampler", sampler);
		matMetal->AddTextureSRV("Albedo", whiteAlbedoSRV);
		matMetal->AddTextureSRV("NormalMap", flatNormalsSRV);
		matMetal->AddTextureSRV("RoughnessMap", roughSRV);
		matMetal->AddTextureSRV("MetalMap", metal1SRV);

		std::shared_ptr<Material> matNonMetal = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1));
		matNonMetal->AddSampler("BasicSampler", sampler);
		matNonMetal->AddTextureSRV("Albedo", whiteAlbedoSRV);
		matNonMetal->AddTextureSRV("NormalMap", flatNormalsSRV);
		matNonMetal->AddTextureSRV("RoughnessMap", roughSRV);
		matNonMetal->AddTextureSRV("MetalMap", metal0SRV);

		// Create the entities
		std::shared_ptr<GameEntity> geMetal = std::make_shared<GameEntity>(sphereMesh, matMetal);
		std::shared_ptr<GameEntity> geNonMetal = std::make_shared<GameEntity>(sphereMesh, matNonMetal);
		entitiesGradient.push_back(geMetal);
		entitiesGradient.push_back(geNonMetal);

		// Move them
		geMetal->GetTransform()->SetPosition(i * 2.0f - 10.0f, 1, 0);
		geNonMetal->GetTransform()->SetPosition(i * 2.0f - 10.0f, -1, 0);
	}
}

// ------------------------------------------------------------
// Resizes (by releasing and re-creating) the resources
// required for post processing.
// 
// We only need to do this at start-up and whenever the 
// window is resized.
// ------------------------------------------------------------
void Game::ResizeAllPostProcessResources()
{
	ResizeOnePostProcessResource(ppRTV, ppSRV, 1.0f, DXGI_FORMAT_R16G16B16A16_FLOAT);
	ResizeOnePostProcessResource(bloomExtractRTV, bloomExtractSRV, 0.5f, DXGI_FORMAT_R16G16B16A16_FLOAT);

	float rtScale = 0.5f;
	for (int i = 0; i < MaxBloomLevels; i++)
	{
		ResizeOnePostProcessResource(blurHorizontalRTV[i], blurHorizontalSRV[i], rtScale);
		ResizeOnePostProcessResource(blurVerticalRTV[i], blurVerticalSRV[i], rtScale);

		// Each successive bloom level is half the resolution
		rtScale *= 0.5f;
	}
}

void Game::ResizeOnePostProcessResource(
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv, 
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, 
	float renderTargetScale,
	DXGI_FORMAT format)
{
	// Reset if they already exist
	rtv.Reset();
	srv.Reset();

	// Describe the render target
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = (unsigned int)(windowWidth * renderTargetScale);
	textureDesc.Height = (unsigned int)(windowHeight * renderTargetScale);
	textureDesc.ArraySize = 1;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Will render to it and sample from it!
	textureDesc.CPUAccessFlags = 0;
	textureDesc.Format = format;
	textureDesc.MipLevels = 1;
	textureDesc.MiscFlags = 0;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> ppTexture;
	device->CreateTexture2D(&textureDesc, 0, ppTexture.GetAddressOf());

	// Create the Render Target View
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = textureDesc.Format;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	device->CreateRenderTargetView(ppTexture.Get(), &rtvDesc, rtv.ReleaseAndGetAddressOf());

	// Create the Shader Resource View using a null description
	// which gives a default SRV with access to the whole resource
	device->CreateShaderResourceView(ppTexture.Get(), 0, srv.ReleaseAndGetAddressOf());
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


void Game::RandomizeEntities()
{
	// Loop through the entities and randomize their positions and sizes
	// Skipping the first, as that's the floor
	for (int i = 1; i < entitiesRandom.size(); i++)
	{
		std::shared_ptr<GameEntity> g = entitiesRandom[i];

		float size = RandomRange(0.1f, 3.0f);
		g->GetTransform()->SetScale(size, size, size);
		g->GetTransform()->SetPosition(
			RandomRange(-25.0f, 25.0f),
			RandomRange(0.0f, 3.0f),
			RandomRange(-25.0f, 25.0f));
	}
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

	// Ensure we resize the post process resources too
	ResizeAllPostProcessResources();
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

	// Check for the all On / all Off switch
	if (input.KeyPress('O'))
	{
		// Are they all already on?
		bool allOn =
			gammaCorrection &&
			useAlbedoTexture &&
			useMetalMap &&
			useNormalMap &&
			useRoughnessMap &&
			usePBR;

		if (allOn)
		{
			gammaCorrection = false;
			useAlbedoTexture = false;
			useMetalMap = false;
			useNormalMap = false;
			useRoughnessMap = false;
			usePBR = false;
		}
		else
		{
			gammaCorrection = true;
			useAlbedoTexture = true;
			useMetalMap = true;
			useNormalMap = true;
			useRoughnessMap = true;
			usePBR = true;
		}
	}

	// Check individual input
	if (input.KeyPress(VK_TAB)) GenerateLights();
	if (input.KeyPress('G')) gammaCorrection = !gammaCorrection;
	if (input.KeyPress('T')) useAlbedoTexture = !useAlbedoTexture;
	if (input.KeyPress('M')) useMetalMap = !useMetalMap;
	if (input.KeyPress('N')) useNormalMap = !useNormalMap;
	if (input.KeyPress('R')) useRoughnessMap = !useRoughnessMap;
	if (input.KeyPress('F')) freezeLightMovement = !freezeLightMovement;
	if (input.KeyPress('L')) drawLights = !drawLights;
	if (input.KeyPress('1')) currentScene = &entitiesLineup;
	if (input.KeyPress('2')) currentScene = &entitiesGradient;
	if (input.KeyPress('3'))
	{
		// If we're already on this scene, randomize it
		if (currentScene == &entitiesRandom) RandomizeEntities();

		// Swap scenes
		currentScene = &entitiesRandom;
	}

	if (input.KeyPress('P'))
	{
		usePBR = !usePBR;
	}

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

	// Handle bloom input
	if (input.KeyDown(VK_LEFT)) { bloomThreshold -= 0.1f * deltaTime; }
	if (input.KeyDown(VK_RIGHT)) { bloomThreshold += 0.1f * deltaTime; }
	bloomThreshold = max(bloomThreshold, 0);

	if (input.KeyPress(VK_OEM_MINUS)) { bloomLevels--; }
	if (input.KeyPress(VK_OEM_PLUS)) { bloomLevels++; }
	bloomLevels = max(min(bloomLevels, MaxBloomLevels), 0);

	if (input.KeyPress('B')) { drawBloomTextures = !drawBloomTextures; }
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

	// --- Post Processing - Pre-Draw ---------------------
	{
		// Clear post process target too
		const float rtClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Black
		context->ClearRenderTargetView(ppRTV.Get(), rtClearColor);
		context->ClearRenderTargetView(bloomExtractRTV.Get(), rtClearColor);

		for (int i = 0; i < MaxBloomLevels; i++)
		{
			context->ClearRenderTargetView(blurHorizontalRTV[i].Get(), rtClearColor);
			context->ClearRenderTargetView(blurVerticalRTV[i].Get(), rtClearColor);
		}

		// Change the render target to the first one for bloom
		context->OMSetRenderTargets(1, ppRTV.GetAddressOf(), depthBufferDSV.Get());
	}

	// Loop through the game entities in the current scene and draw
	Assets& assets = Assets::GetInstance();
	for (auto& e : *currentScene)
	{
		// Ensure each entity has the correct pixel shader
		e->GetMaterial()->SetPixelShader(usePBR ? assets.GetPixelShader(L"PixelShaderPBR") : assets.GetPixelShader(L"PixelShader"));

		// Set total time on this entity's material's pixel shader
		// Note: If the shader doesn't have this variable, nothing happens
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("lightCount", lightCount);
		ps->SetInt("gammaCorrection", (int)gammaCorrection);
		ps->SetInt("useAlbedoTexture", (int)useAlbedoTexture);
		ps->SetInt("useMetalMap", (int)useMetalMap);
		ps->SetInt("useNormalMap", (int)useNormalMap);
		ps->SetInt("useRoughnessMap", (int)useRoughnessMap);

		// Draw one entity
		e->Draw(context, camera);
	}

	// Draw the sky after all regular entities
	sky->Draw(camera);

	// Draw the light sources
	if(drawLights)
		DrawLightSources();

	// --- Post processing - Post-Draw -----------------------
	{
		// Turn OFF vertex and index buffers since we'll be using the
		// full-screen triangle trick
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		ID3D11Buffer* nothing = 0;
		context->IASetIndexBuffer(0, DXGI_FORMAT_R32_UINT, 0);
		context->IASetVertexBuffers(0, 1, &nothing, &stride, &offset);

		// This is the same vertex shader used for all post processing, so set it once
		Assets::GetInstance().GetVertexShader(L"FullscreenVS")->SetShader();

		// Assuming all of the post process steps have a single sampler at register 0
		context->PSSetSamplers(0, 1, ppSampler.GetAddressOf());

		// Handle the bloom extraction
		BloomExtract();

		// Any bloom actually happening?
		if (bloomLevels >= 1)
		{
			float levelScale = 0.5f;
			SingleDirectionBlur(levelScale, XMFLOAT2(1, 0), blurHorizontalRTV[0], bloomExtractSRV); // Bloom extract is the source
			SingleDirectionBlur(levelScale, XMFLOAT2(0, 1), blurVerticalRTV[0], blurHorizontalSRV[0]);

			// Any other levels?
			for (int i = 1; i < bloomLevels; i++)
			{
				levelScale *= 0.5f; // Half the size of the previous
				SingleDirectionBlur(levelScale, XMFLOAT2(1, 0), blurHorizontalRTV[i], blurVerticalSRV[i - 1]); // Previous blur is the source
				SingleDirectionBlur(levelScale, XMFLOAT2(0, 1), blurVerticalRTV[i], blurHorizontalSRV[i]);
			}
		}

		// Final combine
		BloomCombine(); // This step should reset viewport and write to the back buffer since it's the last one

		// Unbind shader resource views at the end of the frame,
		// since we'll be rendering into one of those textures
		// at the start of the next
		ID3D11ShaderResourceView* nullSRVs[16] = {};
		context->PSSetShaderResources(0, 16, nullSRVs);
	}

	// Draw the UI on top of everything
	DrawUI();

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


// --------------------------------------------------------
// Draws a colored sphere at the position of each point light
// --------------------------------------------------------
void Game::DrawLightSources()
{
	Assets& assets = Assets::GetInstance();
	std::shared_ptr<Mesh> lightMesh = assets.GetMesh(L"Models/sphere");
	std::shared_ptr<SimpleVertexShader> vs = assets.GetVertexShader(L"VertexShader");
	std::shared_ptr<SimplePixelShader> ps = assets.GetPixelShader(L"SolidColorPS");

	// Turn on the light mesh
	Microsoft::WRL::ComPtr<ID3D11Buffer> vb = lightMesh->GetVertexBuffer();
	Microsoft::WRL::ComPtr<ID3D11Buffer> ib = lightMesh->GetIndexBuffer();
	unsigned int indexCount = lightMesh->GetIndexCount();

	// Turn on these shaders
	vs->SetShader();
	ps->SetShader();

	// Set up vertex shader
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());

	for (int i = 0; i < lightCount; i++)
	{
		Light light = lights[i];

		// Only drawing point lights here
		if (light.Type != LIGHT_TYPE_POINT)
			continue;

		// Set buffers in the input assembler
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
		context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);

		// Calc quick scale based on range
		float scale = light.Range * light.Range / 200.0f;

		XMMATRIX scaleMat = XMMatrixScaling(scale, scale, scale);
		XMMATRIX transMat = XMMatrixTranslation(light.Position.x, light.Position.y, light.Position.z);

		// Make the transform for this light
		XMFLOAT4X4 world;
		XMStoreFloat4x4(&world, scaleMat * transMat);

		// Set up the world matrix for this light
		vs->SetMatrix4x4("world", world);

		// Set up the pixel shader data
		XMFLOAT3 finalColor = light.Color;
		finalColor.x *= light.Intensity;
		finalColor.y *= light.Intensity;
		finalColor.z *= light.Intensity;
		ps->SetFloat3("Color", finalColor);

		// Copy data
		vs->CopyAllBufferData();
		ps->CopyAllBufferData();

		// Draw
		context->DrawIndexed(indexCount, 0, 0);
	}

}


// --------------------------------------------------------
// Draw the interface
// --------------------------------------------------------
void Game::DrawUI()
{
	// Grab the font from the asset manager
	Assets& assets = Assets::GetInstance();
	std::shared_ptr<SpriteFont> fontArial12 = assets.GetSpriteFont(L"Fonts/Arial12");

	spriteBatch->Begin();

	// Basic controls
	float h = 10.0f;
	fontArial12->DrawString(spriteBatch.get(), L"Controls:", XMVectorSet(10, h, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (WASD, X, Space) Move camera", XMVectorSet(10, h + 20, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (Left Click & Drag) Rotate camera", XMVectorSet(10, h + 40, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (Arrow Up/Down) Increment / decrement lights", XMVectorSet(10, h + 60, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (TAB) Randomize lights", XMVectorSet(10, h + 80, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (F) Freeze/unfreeze lights", XMVectorSet(10, h + 100, 0, 0));

	// Options
	h = 140;
	fontArial12->DrawString(spriteBatch.get(), L"Options: (O) turns all options On/Off", XMVectorSet(10, h, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (G) Gamma Correction:", XMVectorSet(10, h + 20, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (P) Physically-Based:", XMVectorSet(10, h + 40, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (T) Albedo Texture:", XMVectorSet(10, h + 60, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (N) Normal Map:", XMVectorSet(10, h + 80, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (R) Roughness Map:", XMVectorSet(10, h + 100, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (M) Metalness Map:", XMVectorSet(10, h + 120, 0, 0));

	// Current option values
	fontArial12->DrawString(spriteBatch.get(), gammaCorrection ? L"On" : L"Off", XMVectorSet(180, h + 20, 0, 0), gammaCorrection ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));
	fontArial12->DrawString(spriteBatch.get(), usePBR ? L"On" : L"Off", XMVectorSet(180, h + 40, 0, 0), usePBR ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));
	fontArial12->DrawString(spriteBatch.get(), useAlbedoTexture ? L"On" : L"Off", XMVectorSet(180, h + 60, 0, 0), useAlbedoTexture ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));
	fontArial12->DrawString(spriteBatch.get(), useNormalMap ? L"On" : L"Off", XMVectorSet(180, h + 80, 0, 0), useNormalMap ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));
	fontArial12->DrawString(spriteBatch.get(), useRoughnessMap ? L"On" : L"Off", XMVectorSet(180, h + 100, 0, 0), useRoughnessMap ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));
	fontArial12->DrawString(spriteBatch.get(), useMetalMap ? L"On" : L"Off", XMVectorSet(180, h + 120, 0, 0), useMetalMap ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));

	// Light count
	h = 290;
	fontArial12->DrawString(spriteBatch.get(), L"Light Count:", XMVectorSet(10, h, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), std::to_wstring(lightCount).c_str(), XMVectorSet(180, h, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L"(L) Show Point Lights:", XMVectorSet(10, h + 20, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), drawLights ? L"On" : L"Off", XMVectorSet(180, h + 20, 0, 0), drawLights ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));
	fontArial12->DrawString(spriteBatch.get(), L"Press (1, 2, 3) to change scenes", XMVectorSet(10, h + 60, 0, 0));

	// Asset counts
	h = 390;
	fontArial12->DrawString(spriteBatch.get(), L"Asset Manager Stats", XMVectorSet(10, h, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" Meshes: ", XMVectorSet(10, h + 20, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), std::to_wstring(assets.GetMeshCount()).c_str(), XMVectorSet(180, h + 20, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" Textures: ", XMVectorSet(10, h + 40, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), std::to_wstring(assets.GetTextureCount()).c_str(), XMVectorSet(180, h + 40, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" Sprite Fonts: ", XMVectorSet(10, h + 60, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), std::to_wstring(assets.GetSpriteFontCount()).c_str(), XMVectorSet(180, h + 60, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" Pixel Shaders: ", XMVectorSet(10, h + 80, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), std::to_wstring(assets.GetPixelShaderCount()).c_str(), XMVectorSet(180, h + 80, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" Vertex Shader: ", XMVectorSet(10, h + 100, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), std::to_wstring(assets.GetVertexShaderCount()).c_str(), XMVectorSet(180, h + 100, 0, 0));

	// Bloom details
	h = 525;
	std::wstring bloomUI =
		L"Bloom Options\n (-/+) Bloom Levels: " + std::to_wstring(bloomLevels) +
		L"\n (Left/Right) Bloom Threshold: " + std::to_wstring(bloomThreshold) +
		L"\n (B) View post process textures";
	fontArial12->DrawString(spriteBatch.get(), bloomUI.c_str(), XMVectorSet(10, h, 0, 0));
	
	// Draw post process textures?
	if (drawBloomTextures)
	{
		// Relative to screen
		long w = (long)(windowWidth * 0.15f);
		long h = (long)(windowHeight * 0.15f);

		// Where to start the textures
		long xPosLeft = windowWidth - w * 2 - 20;
		long xPosRight = windowWidth - w - 10;

		// Original texture
		RECT pp = { xPosLeft, 10, xPosLeft + w, 10 + h };
		spriteBatch->Draw(ppSRV.Get(), pp);
		fontArial12->DrawString(spriteBatch.get(), L"Original", XMFLOAT2((float)xPosLeft, 10));

		// Extract
		RECT be = { xPosRight, 10, xPosRight + w, 10 + h };
		spriteBatch->Draw(bloomExtractSRV.Get(), be);
		fontArial12->DrawString(spriteBatch.get(), L"Extract", XMFLOAT2((float)xPosRight, 10));

		// Draw each level of bloom
		for (int i = 0; i < bloomLevels; i++)
		{
			long yPos = (i + 1) * (h + 10) + 10;

			// Original texture
			RECT pp = { xPosLeft, yPos, xPosLeft + w, yPos + h };
			spriteBatch->Draw(blurHorizontalSRV[i].Get(), pp);
			fontArial12->DrawString(spriteBatch.get(), (L"H Blur " + std::to_wstring(i)).c_str(), XMFLOAT2((float)xPosLeft, (float)yPos));

			// Extract
			RECT be = { xPosRight, yPos, xPosRight + w, yPos + h };
			spriteBatch->Draw(blurVerticalSRV[i].Get(), be);
			fontArial12->DrawString(spriteBatch.get(), (L"V Blur " + std::to_wstring(i)).c_str(), XMFLOAT2((float)xPosRight, (float)yPos));
		}

	}

	spriteBatch->End();

	// Reset render states, since sprite batch changes these!
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	context->OMSetDepthStencilState(0, 0);

}

// Handles extracting the "bright" pixels to a second render target
void Game::BloomExtract()
{
	// We're using a half-sized texture for bloom extract, so adjust the viewport
	D3D11_VIEWPORT vp = {};
	vp.Width = windowWidth * 0.5f;
	vp.Height = windowHeight * 0.5f;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);

	// Render to the BLOOM EXTRACT texture
	context->OMSetRenderTargets(1, bloomExtractRTV.GetAddressOf(), 0);

	// Grab this shader
	std::shared_ptr<SimplePixelShader> bloomExtractPS = 
		Assets::GetInstance().GetPixelShader(L"BloomExtractPS");

	// Activate the shader and set resources
	bloomExtractPS->SetShader();
	bloomExtractPS->SetShaderResourceView("pixels", ppSRV.Get()); // IMPORTANT: This step takes the original post process texture!
	// Note: Sampler set already!

	// Set post process specific data
	bloomExtractPS->SetFloat("bloomThreshold", bloomThreshold);
	bloomExtractPS->CopyAllBufferData();

	// Draw exactly 3 vertices for our "full screen triangle"
	context->Draw(3, 0);
}


// Blurs in a single direction, based on the "blurDirection" parameter
// This allows us to use a single shader for both horizontal and vertical
// blurring, rather than having to write two nearly-identical shaders
void Game::SingleDirectionBlur(float renderTargetScale, DirectX::XMFLOAT2 blurDirection, Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sourceTexture)
{
	// Ensure our viewport matches our render target
	D3D11_VIEWPORT vp = {};
	vp.Width = windowWidth * renderTargetScale;
	vp.Height = windowHeight * renderTargetScale;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);

	// Target to which we're rendering
	context->OMSetRenderTargets(1, target.GetAddressOf(), 0);

	// Grab this shader
	std::shared_ptr<SimplePixelShader> gaussianBlurPS =
		Assets::GetInstance().GetPixelShader(L"GaussianBlurPS");


	// Activate the shader and set resources
	gaussianBlurPS->SetShader();
	gaussianBlurPS->SetShaderResourceView("pixels", sourceTexture.Get()); // The texture from the previous step
	// Note: Sampler set already!

	// Set post process specific data
	gaussianBlurPS->SetFloat2("pixelUVSize", XMFLOAT2(1.0f / (windowWidth * renderTargetScale), 1.0f / (windowHeight * renderTargetScale)));
	gaussianBlurPS->SetFloat2("blurDirection", blurDirection);
	gaussianBlurPS->CopyAllBufferData();

	// Draw exactly 3 vertices for our "full screen triangle"
	context->Draw(3, 0);
}

// Combines all bloom levels with the original post process target
// Note: If a level isn't being used, it's still cleared to black
//       so it won't have any impact on the final result
void Game::BloomCombine()
{
	// Back to the full window viewport
	D3D11_VIEWPORT vp = {};
	vp.Width = (float)windowWidth;
	vp.Height = (float)windowHeight;
	vp.MaxDepth = 1.0f;
	context->RSSetViewports(1, &vp);

	// Render to the BACK BUFFER (since this is the last step!)
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), 0);

	// Grab this shader
	std::shared_ptr<SimplePixelShader> bloomCombinePS =
		Assets::GetInstance().GetPixelShader(L"BloomCombinePS");

	// Activate the shader and set resources
	bloomCombinePS->SetShader();
	bloomCombinePS->SetShaderResourceView("originalPixels", ppSRV.Get()); // Set the original render
	bloomCombinePS->SetShaderResourceView("bloomedPixels0", blurVerticalSRV[0].Get()); // And all other bloom levels
	bloomCombinePS->SetShaderResourceView("bloomedPixels1", blurVerticalSRV[1].Get()); // And all other bloom levels
	bloomCombinePS->SetShaderResourceView("bloomedPixels2", blurVerticalSRV[2].Get()); // And all other bloom levels
	bloomCombinePS->SetShaderResourceView("bloomedPixels3", blurVerticalSRV[3].Get()); // And all other bloom levels
	bloomCombinePS->SetShaderResourceView("bloomedPixels4", blurVerticalSRV[4].Get()); // And all other bloom levels

	// Note: Sampler set already!

	// Set post process specific data
	bloomCombinePS->SetFloat("intensityLevel0", bloomLevelIntensities[0]);
	bloomCombinePS->SetFloat("intensityLevel1", bloomLevelIntensities[1]);
	bloomCombinePS->SetFloat("intensityLevel2", bloomLevelIntensities[2]);
	bloomCombinePS->SetFloat("intensityLevel3", bloomLevelIntensities[3]);
	bloomCombinePS->SetFloat("intensityLevel4", bloomLevelIntensities[4]);
	bloomCombinePS->CopyAllBufferData();

	// Draw exactly 3 vertices for our "full screen triangle"
	context->Draw(3, 0);
}

