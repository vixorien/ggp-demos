#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Assets.h"
#include "Helpers.h"

#include "WICTextureLoader.h"

#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)
#include <algorithm>


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
	gammaCorrection(true),
	useAlbedoTexture(true),
	useMetalMap(true),
	useNormalMap(true),
	useRoughnessMap(true),
	usePBR(true),
	drawLights(true),
	freezeLightMovement(false),
	lightCount(3),
	transparencyOn(true),
	renderTransparentBackfaces(true),
	sortTransparentObjects(true)
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

	// Loading scene stuff
	LoadAssetsAndCreateEntities();

	// Set up lights
	lightCount = 3;
	GenerateLights();
	
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

	// Transparency render states

	// Blend state for standard alpha blending
	//  Source blend is Source Alpha
	//  Dest blend is Inverse Source Alpha (1 - srcAlpha)
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = true;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	device->CreateBlendState(&blendDesc, alphaBlendState.GetAddressOf());

	// Rasterizer state to render back faces
	D3D11_RASTERIZER_DESC rastDesc = {};
	rastDesc.DepthClipEnable = true;
	rastDesc.CullMode = D3D11_CULL_FRONT;
	rastDesc.FillMode = D3D11_FILL_SOLID;
	device->CreateRasterizerState(&rastDesc, backfaceRasterState.GetAddressOf());

	// Create the sky (loading custom shaders in-line below)
	sky = std::make_shared<Sky>(
		FixPath(L"../../../Assets/Skies/Clouds Blue/right.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/left.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/up.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/down.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/front.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/back.png").c_str(),
		assets.GetMesh(L"Models/cube"),
		assets.GetVertexShader(L"SkyVS"),
		assets.GetPixelShader(L"SkyPS"),
		sampler,
		device,
		context);

	// Grab shaders needed below
	std::shared_ptr<SimpleVertexShader> vertexShader = assets.GetVertexShader(L"VertexShader");
	std::shared_ptr<SimplePixelShader> pixelShader = assets.GetPixelShader(L"PixelShaderPBR");

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

	// Create transparent materials
	std::shared_ptr<Material> fenceMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 1), XMFLOAT2(0, 0), true);
	fenceMat->AddSampler("BasicSampler", sampler);
	fenceMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/Transparent/fence_albedo"));
	fenceMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/Transparent/fence_normals"));
	fenceMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/Transparent/fence_roughness"));
	fenceMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/Transparent/fence_metal"));

	std::shared_ptr<Material> latticeMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 0.5f), XMFLOAT2(0, 0), true);
	latticeMat->AddSampler("BasicSampler", sampler);
	latticeMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/Transparent/lattice_albedo"));
	latticeMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/Transparent/lattice_normals"));
	latticeMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/Transparent/lattice_roughness"));
	latticeMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/Transparent/lattice_metal"));

	std::shared_ptr<Material> glassWindowMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 0.5f), XMFLOAT2(0, 0), true);
	glassWindowMat->AddSampler("BasicSampler", sampler);
	glassWindowMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/Transparent/glass_window_albedo"));
	glassWindowMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/Transparent/glass_window_normals"));
	glassWindowMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/Transparent/glass_window_roughness"));
	glassWindowMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/Transparent/glass_window_metal"));

	std::shared_ptr<Material> glassPatternMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 1), XMFLOAT2(0, 0), true);
	glassPatternMat->AddSampler("BasicSampler", sampler);
	glassPatternMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/Transparent/glass_pattern_albedo"));
	glassPatternMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/Transparent/glass_pattern_normals"));
	glassPatternMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/Transparent/glass_pattern_roughness"));
	glassPatternMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/Transparent/glass_pattern_metal"));

	// Get meshes needed below
	std::shared_ptr<Mesh> cubeMesh = assets.GetMesh(L"Models/cube");
	std::shared_ptr<Mesh> sphereMesh = assets.GetMesh(L"Models/sphere");


	// === Create the "randomized" entities, with a static floor ===========
	std::shared_ptr<GameEntity> floor = std::make_shared<GameEntity>(cubeMesh, cobbleMat4x);
	floor->GetTransform()->SetScale(50, 50, 50);
	floor->GetTransform()->SetPosition(0, -27, 0);
	entities.push_back(floor);

	for (int i = 0; i < 32; i++)
	{
		std::shared_ptr<Material> whichMat = floorMat;
		switch (i % 11)
		{
		case 0: whichMat = floorMat; break;
		case 1: whichMat = paintMat; break;
		case 2: whichMat = cobbleMat2x; break;
		case 3: whichMat = scratchedMat; break;
		case 4: whichMat = bronzeMat; break;
		case 5: whichMat = roughMat; break;
		case 6: whichMat = woodMat; break;
		case 7: whichMat = fenceMat; break;
		case 8: whichMat = latticeMat; break;
		case 9: whichMat = glassPatternMat; break;
		case 10: whichMat = glassWindowMat; break;
		}

		std::shared_ptr<GameEntity> sphere = std::make_shared<GameEntity>(sphereMesh, whichMat);
		entities.push_back(sphere);
	}

	RandomizeEntities();

	
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
	for (int i = 1; i < entities.size(); i++)
	{
		std::shared_ptr<GameEntity> g = entities[i];

		float size = RandomRange(1.0f, 3.0f);
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
	if (input.KeyPress('P')) usePBR = !usePBR;
	if (input.KeyPress('E')) RandomizeEntities();

	// Transparency options
	if (input.KeyPress('1')) transparencyOn = !transparencyOn;
	if (input.KeyPress('2')) sortTransparentObjects = !sortTransparentObjects;
	if (input.KeyPress('3')) renderTransparentBackfaces = !renderTransparentBackfaces;


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

	// Make a list of transparent entities, which need to be skipped when initially drawing
	std::vector<std::shared_ptr<GameEntity>> transparentEntities;

	// Loop through the game entities in the current scene and draw
	Assets& assets = Assets::GetInstance();
	for (auto& e : entities)
	{
		// If this entity's material is transparent, we
		// need to skip drawing it for now
		if (e->GetMaterial()->GetTransparent())
		{
			transparentEntities.push_back(e);
			continue;
		}

		DrawOneEntity(e);
	}

	// Draw the sky after all regular entities
	sky->Draw(camera);

	// Draw the light sources
	if(drawLights)
		DrawLightSources();

	// Sort the transparent objects by distance to the camera
	if (sortTransparentObjects)
	{
		// Sort using a lambda function
		std::sort(
			transparentEntities.begin(), 
			transparentEntities.end(), 
			[&](std::shared_ptr<GameEntity> a, std::shared_ptr<GameEntity> b) -> bool
			{
				// Grab vectors
				XMFLOAT3 aPos = a->GetTransform()->GetPosition();
				XMFLOAT3 bPos = b->GetTransform()->GetPosition();
				XMFLOAT3 camPos = camera->GetTransform()->GetPosition();

				// Calc distances and compare
				float aDist = XMVectorGetX(XMVector3Length(XMLoadFloat3(&aPos) - XMLoadFloat3(&camPos)));
				float bDist = XMVectorGetX(XMVector3Length(XMLoadFloat3(&bPos) - XMLoadFloat3(&camPos)));
				return aDist > bDist;
			});
	}

	// Transparency
	{
		// Turn on our alpha blend state if necessary
		if (transparencyOn) 
			context->OMSetBlendState(alphaBlendState.Get(), 0, 0xFFFFFFFF);
		
		// Render all transparent objects
		for (auto& e : transparentEntities)
		{
			// Draw insides if necessary, flipping the normal
			if (renderTransparentBackfaces)
			{
				context->RSSetState(backfaceRasterState.Get());
				DrawOneEntity(e, true);
				context->RSSetState(0);
			}

			// Draw the front faces of this entity
			DrawOneEntity(e);
		}
		// Disable transparency afterwards
		if (transparencyOn) 
			context->OMSetBlendState(0, 0, 0xFFFFFFFF);
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
// Draws a single entity
// 
// entity - The entity to draw
// flipNormal - Should the normal be flipped (inverted)?  This
//              is mainly used when rendering the inside of
//              an object, often with transparency
// --------------------------------------------------------
void Game::DrawOneEntity(std::shared_ptr<GameEntity> entity, bool flipNormal)
{
	// Ensure each entity has the correct pixel shader
	Assets& assets = Assets::GetInstance();
	entity->GetMaterial()->SetPixelShader(usePBR ? assets.GetPixelShader(L"PixelShaderPBR") : assets.GetPixelShader(L"PixelShader"));

	// Set total time on this entity's material's pixel shader
	// Note: If the shader doesn't have this variable, nothing happens
	std::shared_ptr<SimplePixelShader> ps = entity->GetMaterial()->GetPixelShader();
	ps->SetFloat3("ambientColor", ambientColor);
	ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
	ps->SetInt("lightCount", lightCount);
	ps->SetInt("gammaCorrection", (int)gammaCorrection);
	ps->SetInt("useAlbedoTexture", (int)useAlbedoTexture);
	ps->SetInt("useMetalMap", (int)useMetalMap);
	ps->SetInt("useNormalMap", (int)useNormalMap);
	ps->SetInt("useRoughnessMap", (int)useRoughnessMap);
	ps->SetInt("flipNormal", (int)flipNormal);

	// Draw one entity
	entity->Draw(context, camera);
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
	fontArial12->DrawString(spriteBatch.get(), L" (E) Randomize entities", XMVectorSet(10, h + 120, 0, 0));

	// Options
	h = 160;
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
	h = 310;
	fontArial12->DrawString(spriteBatch.get(), L"Light Count:", XMVectorSet(10, h, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), std::to_wstring(lightCount).c_str(), XMVectorSet(180, h, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L"(L) Show Point Lights:", XMVectorSet(10, h + 20, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), drawLights ? L"On" : L"Off", XMVectorSet(180, h + 20, 0, 0), drawLights ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));

	// Transparency options
	h = 390;
	fontArial12->DrawString(spriteBatch.get(), L"== Transparency Options ==", XMVectorSet(10, h, 0, 0));

	fontArial12->DrawString(spriteBatch.get(), L" (1) Transparency:", XMVectorSet(10, h + 20, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), transparencyOn ? L"On" : L"Off", XMVectorSet(250, h + 20, 0, 0), transparencyOn ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));
	fontArial12->DrawString(spriteBatch.get(), L" (2) Sort Transparent Objects:", XMVectorSet(10, h + 40, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), sortTransparentObjects ? L"On" : L"Off", XMVectorSet(250, h + 40, 0, 0), sortTransparentObjects ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));
	fontArial12->DrawString(spriteBatch.get(), L" (3) Draw Transparent Backfaces:", XMVectorSet(10, h + 60, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), renderTransparentBackfaces ? L"On" : L"Off", XMVectorSet(250, h + 60, 0, 0), renderTransparentBackfaces ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));

	spriteBatch->End();

	// Reset render states, since sprite batch changes these!
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	context->OMSetDepthStencilState(0, 0);

}

