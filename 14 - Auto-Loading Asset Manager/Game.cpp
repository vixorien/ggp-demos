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
	lightCount(3)
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
	for (auto& e : entitiesRandom) delete e;
	for (auto& e : entitiesLineup) delete e;
	for (auto& e : entitiesGradient) delete e;

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

	// Set the current scene (which of the 3 lists of entities are we drawing)
	currentScene = &entitiesLineup;

	// Loading scene stuff
	LoadAssetsAndCreateEntities();

	// Set up lights
	lightCount = 3;
	GenerateLights();
	
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
	// Initialize the asset manager and load all assets
	Assets& assets = Assets::GetInstance();
	assets.Initialize("../../../Assets/", device, context, true);
	assets.LoadAllAssets();

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
	sky = new Sky(
		GetFullPathTo_Wide(L"../../../Assets/Skies/Night Moon/right.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Night Moon/left.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Night Moon/up.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Night Moon/down.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Night Moon/front.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Night Moon/back.png").c_str(),
		assets.GetMesh("Models/cube"),
		assets.GetVertexShader("SkyVS"),
		assets.GetPixelShader("SkyPS"),
		sampler,
		device,
		context);


	// Grab shaders needed below
	std::shared_ptr<SimpleVertexShader> vertexShader = assets.GetVertexShader("VertexShader");
	std::shared_ptr<SimplePixelShader> pixelShader = assets.GetPixelShader("PixelShader");

	// Create basic materials
	Material* cobbleMat2x = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	cobbleMat2x->AddSampler("BasicSampler", sampler);
	cobbleMat2x->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/cobblestone_albedo"));
	cobbleMat2x->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/cobblestone_normals"));
	cobbleMat2x->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/cobblestone_roughness"));
	cobbleMat2x->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/cobblestone_metal"));
	
	Material* cobbleMat4x = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4x->AddSampler("BasicSampler", sampler);
	cobbleMat4x->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/cobblestone_albedo"));
	cobbleMat4x->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/cobblestone_normals"));
	cobbleMat4x->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/cobblestone_roughness"));
	cobbleMat4x->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/cobblestone_metal"));

	Material* floorMat = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	floorMat->AddSampler("BasicSampler", sampler);
	floorMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/floor_albedo"));
	floorMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/floor_normals"));
	floorMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/floor_roughness"));
	floorMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/floor_metal"));

	Material* paintMat = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	paintMat->AddSampler("BasicSampler", sampler);
	paintMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/paint_albedo"));
	paintMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/paint_normals"));
	paintMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/paint_roughness"));
	paintMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/paint_metal"));

	Material* scratchedMat = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	scratchedMat->AddSampler("BasicSampler", sampler);
	scratchedMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/scratched_albedo"));
	scratchedMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/scratched_normals"));
	scratchedMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/scratched_roughness"));
	scratchedMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/scratched_metal"));

	Material* bronzeMat = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	bronzeMat->AddSampler("BasicSampler", sampler);
	bronzeMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/bronze_albedo"));
	bronzeMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/bronze_normals"));
	bronzeMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/bronze_roughness"));
	bronzeMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/bronze_metal"));

	Material* roughMat = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	roughMat->AddSampler("BasicSampler", sampler);
	roughMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/rough_albedo"));
	roughMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/rough_normals"));
	roughMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/rough_roughness"));
	roughMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/rough_metal"));

	Material* woodMat = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	woodMat->AddSampler("BasicSampler", sampler);
	woodMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/wood_albedo"));
	woodMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/wood_normals"));
	woodMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/wood_roughness"));
	woodMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/wood_metal"));


	materials.push_back(cobbleMat2x);
	materials.push_back(cobbleMat4x);
	materials.push_back(floorMat);
	materials.push_back(paintMat);
	materials.push_back(scratchedMat);
	materials.push_back(bronzeMat);
	materials.push_back(roughMat);
	materials.push_back(woodMat);


	// Get meshes needed below
	Mesh* cubeMesh = assets.GetMesh("Models/cube");
	Mesh* sphereMesh = assets.GetMesh("Models/sphere");


	// === Create the "randomized" entities, with a static floor ===========
	GameEntity* floor = new GameEntity(cubeMesh, cobbleMat4x);
	floor->GetTransform()->SetScale(50, 50, 50);
	floor->GetTransform()->SetPosition(0, -27, 0);
	entitiesRandom.push_back(floor);

	for (int i = 0; i < 32; i++)
	{
		Material* whichMat = floorMat;
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

		GameEntity* sphere = new GameEntity(sphereMesh, whichMat);
		sphere->GetTransform()->SetScale(size, size, size);
		sphere->GetTransform()->SetPosition(
			RandomRange(-25.0f, 25.0f),
			RandomRange(0.0f, 3.0f),
			RandomRange(-25.0f, 25.0f));

		entitiesRandom.push_back(sphere);
	}



	// === Create the line up entities =====================================
	GameEntity* cobSphere = new GameEntity(sphereMesh, cobbleMat2x);
	cobSphere->GetTransform()->SetPosition(-6, 0, 0);

	GameEntity* floorSphere = new GameEntity(sphereMesh, floorMat);
	floorSphere->GetTransform()->SetPosition(-4, 0, 0);

	GameEntity* paintSphere = new GameEntity(sphereMesh, paintMat);
	paintSphere->GetTransform()->SetPosition(-2, 0, 0);

	GameEntity* scratchSphere = new GameEntity(sphereMesh, scratchedMat);
	scratchSphere->GetTransform()->SetPosition(0, 0, 0);

	GameEntity* bronzeSphere = new GameEntity(sphereMesh, bronzeMat);
	bronzeSphere->GetTransform()->SetPosition(2, 0, 0);

	GameEntity* roughSphere = new GameEntity(sphereMesh, roughMat);
	roughSphere->GetTransform()->SetPosition(4, 0, 0);

	GameEntity* woodSphere = new GameEntity(sphereMesh, woodMat);
	woodSphere->GetTransform()->SetPosition(6, 0, 0);

	entitiesLineup.push_back(cobSphere);
	entitiesLineup.push_back(floorSphere);
	entitiesLineup.push_back(paintSphere);
	entitiesLineup.push_back(scratchSphere);
	entitiesLineup.push_back(bronzeSphere);
	entitiesLineup.push_back(roughSphere);
	entitiesLineup.push_back(woodSphere);



	// === Create a gradient of entities based on roughness & metalness ====
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteAlbedoSRV = assets.CreateSolidColorTexture("Textures/WhiteAlbedo", 2, 2, XMFLOAT4(1, 1, 1, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metal0SRV = assets.CreateSolidColorTexture("Textures/Metal0", 2, 2, XMFLOAT4(0, 0, 0, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metal1SRV = assets.CreateSolidColorTexture("Textures/Metal1", 2, 2, XMFLOAT4(1, 1, 1, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> flatNormalsSRV = assets.CreateSolidColorTexture("Textures/FlatNormals", 2, 2, XMFLOAT4(0.5f, 0.5f, 1.0f, 1));

	for (int i = 0; i <= 10; i++)
	{
		// Roughness value for this entity
		float r = i / 10.0f;

		// Create textures
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughSRV = assets.CreateSolidColorTexture("Textures/Rough" + std::to_string(r),2, 2, XMFLOAT4(r, r, r, 1));

		// Set up the materials
		Material* matMetal = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1));
		matMetal->AddSampler("BasicSampler", sampler);
		matMetal->AddTextureSRV("Albedo", whiteAlbedoSRV);
		matMetal->AddTextureSRV("NormalMap", flatNormalsSRV);
		matMetal->AddTextureSRV("RoughnessMap", roughSRV);
		matMetal->AddTextureSRV("MetalMap", metal1SRV);
		materials.push_back(matMetal);

		Material* matNonMetal = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1));
		matNonMetal->AddSampler("BasicSampler", sampler);
		matNonMetal->AddTextureSRV("Albedo", whiteAlbedoSRV);
		matNonMetal->AddTextureSRV("NormalMap", flatNormalsSRV);
		matNonMetal->AddTextureSRV("RoughnessMap", roughSRV);
		matNonMetal->AddTextureSRV("MetalMap", metal0SRV);
		materials.push_back(matNonMetal);

		// Create the entities
		GameEntity* geMetal = new GameEntity(sphereMesh, matMetal);
		GameEntity* geNonMetal = new GameEntity(sphereMesh, matNonMetal);
		entitiesGradient.push_back(geMetal);
		entitiesGradient.push_back(geNonMetal);

		// Move them
		geMetal->GetTransform()->SetPosition(i * 2.0f - 10.0f, 1, 0);
		geNonMetal->GetTransform()->SetPosition(i * 2.0f - 10.0f, -1, 0);
	}
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
		GameEntity* g = entitiesRandom[i];

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
	if (camera) camera->UpdateProjectionMatrix((float)width / height);
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

		// Since we're changing states, handle the PBR shader swap
		std::shared_ptr<SimplePixelShader> psToUse = usePBR ? assets.GetPixelShader("PixelShaderPBR") : assets.GetPixelShader("PixelShader");
		for (auto& m : materials)
			m->SetPixelShader(psToUse);
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
		std::shared_ptr<SimplePixelShader> psToUse = usePBR ? assets.GetPixelShader("PixelShaderPBR") : assets.GetPixelShader("PixelShader");
		for (auto& m : materials)
			m->SetPixelShader(psToUse);
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


	// Loop through the game entities in the current scene and draw
	for (auto& e : *currentScene)
	{
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



void Game::DrawLightSources()
{
	Assets& assets = Assets::GetInstance();
	Mesh* lightMesh = assets.GetMesh("Models/sphere");
	std::shared_ptr<SimpleVertexShader> vs = assets.GetVertexShader("VertexShader");
	std::shared_ptr<SimplePixelShader> ps = assets.GetPixelShader("SolidColorPS");

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

void Game::DrawUI()
{
	// Grab the font from the asset manager
	Assets& assets = Assets::GetInstance();
	std::shared_ptr<SpriteFont> fontArial12 = assets.GetSpriteFont("Fonts/Arial12");

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

	spriteBatch->End();

	// Reset render states, since sprite batch changes these!
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	context->OMSetDepthStencilState(0, 0);

}