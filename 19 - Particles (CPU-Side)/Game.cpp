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
	for (auto& e : entities) delete e;
	for (auto& e : emitters) delete e;

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
	std::shared_ptr<SimplePixelShader> pixelShader = assets.GetPixelShader("PixelShaderPBR");

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

	Material* woodMat = new Material(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
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


	// === Create the scene ===
	GameEntity* sphere = new GameEntity(assets.GetMesh("Models/sphere"), scratchedMat);
	sphere->GetTransform()->SetPosition(-5, 0, 0);
	entities.push_back(sphere);

	GameEntity* helix = new GameEntity(assets.GetMesh("Models/helix"), paintMat);
	entities.push_back(helix);

	GameEntity* cube = new GameEntity(assets.GetMesh("Models/cube"), woodMat);
	cube->GetTransform()->SetPosition(5, 0, 0);
	cube->GetTransform()->SetScale(2, 2, 2);
	entities.push_back(cube);

	// Particle states
	// A depth state for the particles
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	dsDesc.DepthEnable = true;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // Turns off depth writing
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
	device->CreateDepthStencilState(&dsDesc, particleDepthState.GetAddressOf());


	// Blend for particles (additive)
	D3D11_BLEND_DESC blend = {};
	blend.AlphaToCoverageEnable = false;
	blend.IndependentBlendEnable = false;
	blend.RenderTarget[0].BlendEnable = true;
	blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA; // Still respect pixel shader output alpha
	blend.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	device->CreateBlendState(&blend, particleBlendState.GetAddressOf());

	// Debug rasterizer state for particles
	D3D11_RASTERIZER_DESC rd = {};
	rd.CullMode = D3D11_CULL_BACK;
	rd.DepthClipEnable = true;
	rd.FillMode = D3D11_FILL_WIREFRAME;
	device->CreateRasterizerState(&rd, particleDebugRasterState.GetAddressOf());

	// Grab loaded particle resources
	std::shared_ptr<SimpleVertexShader> particleVS = assets.GetVertexShader("ParticleVS");
	std::shared_ptr<SimplePixelShader> particlePS = assets.GetPixelShader("ParticlePS");

	// Create particle materials
	Material* fireParticle = new Material(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	fireParticle->AddSampler("BasicSampler", sampler);
	fireParticle->AddTextureSRV("Particle", assets.GetTexture("Textures/Particles/Black/fire_01"));

	Material* twirlParticle = new Material(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	twirlParticle->AddSampler("BasicSampler", sampler);
	twirlParticle->AddTextureSRV("Particle", assets.GetTexture("Textures/Particles/Black/twirl_02"));

	Material* starParticle = new Material(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	starParticle->AddSampler("BasicSampler", sampler);
	starParticle->AddTextureSRV("Particle", assets.GetTexture("Textures/Particles/Black/star_04"));

	Material* animParticle = new Material(particlePS, particleVS, XMFLOAT3(1, 1, 1));
	animParticle->AddSampler("BasicSampler", sampler);
	animParticle->AddTextureSRV("Particle", assets.GetTexture("Textures/Particles/flame_animated"));

	// Save for cleanup
	materials.push_back(fireParticle);
	materials.push_back(twirlParticle);
	materials.push_back(starParticle);
	materials.push_back(animParticle);

	// Create example emitters
	
	// Flame thrower
	emitters.push_back(new Emitter(
		160,							// Max particles
		30,								// Particles per second
		5,								// Particle lifetime
		0.1f,							// Start size
		4.0f,							// End size
		XMFLOAT4(1, 0.1f, 0.1f, 0.7f),	// Start color
		XMFLOAT4(1, 0.6f, 0.1f, 0),		// End color
		XMFLOAT3(-2, 2, 0),				// Start velocity
		XMFLOAT3(0.2f, 0.2f, 0.2f),		// Velocity randomness range
		XMFLOAT3(2, 0, 0),				// Emitter position
		XMFLOAT3(0.1f, 0.1f, 0.1f),		// Position randomness range
		XMFLOAT4(-2, 2, -2, 2),			// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, -1, 0),				// Constant acceleration
		device,
		fireParticle));

	// Erratic swirly portal
	emitters.push_back(new Emitter(
		45,								// Max particles
		20,								// Particles per second
		2,								// Particle lifetime
		3.0f,							// Start size
		2.0f,							// End size
		XMFLOAT4(0.2f, 0.1f, 0.1f, 0.0f),// Start color
		XMFLOAT4(0.2f, 0.7f, 0.1f, 1.0f),// End color
		XMFLOAT3(0, 0, 0),				// Start velocity
		XMFLOAT3(0, 0, 0),				// Velocity randomness range
		XMFLOAT3(3.5f, 3.5f, 0),		// Emitter position
		XMFLOAT3(0, 0, 0),				// Position randomness range
		XMFLOAT4(-5, 5, -5, 5),			// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, 0, 0),				// Constant acceleration
		device,
		twirlParticle));

	// Falling star field
	emitters.push_back(new Emitter(
		250,							// Max particles
		100,							// Particles per second
		2,								// Particle lifetime
		2.0f,							// Start size
		0.0f,							// End size
		XMFLOAT4(0.1f, 0.2f, 0.5f, 0.0f),// Start color
		XMFLOAT4(0.1f, 0.1f, 0.3f, 3.0f),// End color (ending with high alpha so we hit 1.0 sooner)
		XMFLOAT3(0, 0, 0),				// Start velocity
		XMFLOAT3(0.1f, 0, 0.1f),		// Velocity randomness range
		XMFLOAT3(-2.5f, -1, 0),			// Emitter position
		XMFLOAT3(1, 0, 1),				// Position randomness range
		XMFLOAT4(0, 0, -3, 3),			// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, -2, 0),				// Constant acceleration
		device,
		starParticle));


	emitters.push_back(new Emitter(
		5,						// Max particles
		2,						// Particles per second
		2,						// Particle lifetime
		1.0f,					// Start size
		1.0f,					// End size
		XMFLOAT4(1, 1, 1, 1),	// Start color
		XMFLOAT4(1, 1, 1, 0),	// End color
		XMFLOAT3(0, 0, 0),		// Start velocity
		XMFLOAT3(0, 0, 0),		// Velocity randomness range
		XMFLOAT3(2, -2, 0),		// Emitter position
		XMFLOAT3(0, 0, 0),		// Position randomness range
		XMFLOAT4(-2, 2, -2, 2),	// Random rotation ranges (startMin, startMax, endMin, endMax)
		XMFLOAT3(0, 0, 0),		// Constant acceleration
		device,
		animParticle,
		true,
		8,
		8));



}


void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(1, -1, 1);
	dir1.Color = XMFLOAT3(1, 1, 1);
	dir1.Intensity = 1.0f;
	dir1.CastsShadows = 1; // 0 = false, 1 = true

	Light dir2 = {};
	dir2.Type = LIGHT_TYPE_DIRECTIONAL;
	dir2.Direction = XMFLOAT3(-1, -0.25f, 0);
	dir2.Color = XMFLOAT3(1, 1, 1);
	dir2.Intensity = 1.0f;

	Light dir3 = {};
	dir3.Type = LIGHT_TYPE_DIRECTIONAL;
	dir3.Direction = XMFLOAT3(0, -1, 1);
	dir3.Color = XMFLOAT3(1, 1, 1);
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

	// Since Init() takes a while, the first deltaTime
	// ends up being a massive number, which ends up emitting
	// a ton of particles.  Skipping the very first frame!
	static bool firstFrame = true; // Only ever initialized once due to static
	if (firstFrame) { deltaTime = 0.0f; firstFrame = false; }

	// Update all emitters
	for (auto& e : emitters)
	{
		e->Update(deltaTime);
	}

	// Handle light count changes, clamped appropriately
	if (input.KeyDown('R')) lightCount = 3;
	if (input.KeyDown(VK_UP)) lightCount++;
	if (input.KeyDown(VK_DOWN)) lightCount--;
	lightCount = max(1, min(MAX_LIGHTS, lightCount));

	// Move lights
	for (int i = 0; i < lightCount; i++)
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
	context->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,	1.0f, 0);

	// Loop through the game entities in the current scene and draw
	for (auto& e : entities)
	{
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("lightCount", lightCount);

		// Draw one entity
		e->Draw(context, camera);
	}

	// Draw the sky after all regular entities
	sky->Draw(camera);

	// Draw all emitters
	DrawParticles();

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
	fontArial12->DrawString(spriteBatch.get(), L" (C) Particle wireframe", XMVectorSet(10, h + 100, 0, 0));
	

	spriteBatch->End();

	// Reset render states, since sprite batch changes these!
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	context->OMSetDepthStencilState(0, 0);

}

void Game::DrawParticles()
{
	// Particle drawing =============
	{

		// Particle states
		context->OMSetBlendState(particleBlendState.Get(), 0, 0xffffffff);	// Additive blending
		context->OMSetDepthStencilState(particleDepthState.Get(), 0);		// No depth WRITING

		// Draw all of the emitters
		for (auto& e : emitters)
		{
			e->Draw(context, camera, false);
		}

		// Should we also draw them in wireframe?
		if (Input::GetInstance().KeyDown('C'))
		{
			context->RSSetState(particleDebugRasterState.Get());
			for (auto& e : emitters)
			{
				e->Draw(context, camera, true);
			}
		}

		// Reset to default states for next frame
		context->OMSetBlendState(0, 0, 0xffffffff);
		context->OMSetDepthStencilState(0, 0);
		context->RSSetState(0);
	}
}
