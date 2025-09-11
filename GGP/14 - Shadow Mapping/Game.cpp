#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "UIHelpers.h"
#include "AssetPath.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "WICTextureLoader.h"

#include <DirectXMath.h>

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// --------------------------------------------------------
// Called once per program, after the window and graphics API
// are initialized but before the game loop begins
// --------------------------------------------------------
void Game::Initialize()
{
	// Initialize ImGui itself & platform/renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(Window::Handle());
	ImGui_ImplDX11_Init(Graphics::Device.Get(), Graphics::Context.Get());
	ImGui::StyleColorsDark();

	// Seed random
	srand((unsigned int)time(0));

	// Lighting options
	lightOptions = {
		.LightCount = 10,
		.FreezeLightMovement = true,
		.LightMoveTime = 0.0f,
		.FreezeEntityMovement = false,
		.EntityMoveTime = 0.0f,
		.DrawLights = true,
		.AmbientColor{0,0,0}
	};

	// Set up the scene and create lights
	LoadAssetsAndCreateEntities();
	GenerateLights();

	// Set initial graphics API state
	Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the camera
	camera = std::make_shared<FPSCamera>(
		XMFLOAT3(0.0f, 3.0f, -25.0f),	// Position
		5.0f,					// Move speed
		0.002f,					// Look speed
		XM_PIDIV4,				// Field of view
		Window::AspectRatio(),  // Aspect ratio
		0.01f,					// Near clip
		100.0f,					// Far clip
		CameraProjectionType::Perspective);
	
	// Shadow map setup
	shadowOptions.ShadowMapResolution = 1024;
	shadowOptions.ShadowProjectionSize = 10.0f;
	CreateShadowMapResources();
}


// --------------------------------------------------------
// Clean up memory or objects created by this class
// 
// Note: Using smart pointers means there probably won't
//       be much to manually clean up here!
// --------------------------------------------------------
Game::~Game()
{
	// ImGui clean up
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}


// --------------------------------------------------------
// Loads assets and creates the geometry we're going to draw
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
	Graphics::Device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());

	// Load textures
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobbleA, cobbleN, cobbleR, cobbleM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorA, floorN, floorR, floorM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> paintA, paintN, paintR, paintM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedA, scratchedN, scratchedR, scratchedM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeA, bronzeN, bronzeR, bronzeM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughA, roughN, roughR, roughM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodA, woodN, woodR, woodM;

	// Quick pre-processor macro for simplifying texture loading calls below
#define LoadTexture(path, srv) CreateWICTextureFromFile(Graphics::Device.Get(), Graphics::Context.Get(), FixPath(path).c_str(), 0, srv.GetAddressOf());
	LoadTexture(AssetPath + L"Textures/PBR/cobblestone_albedo.png", cobbleA);
	LoadTexture(AssetPath + L"Textures/PBR/cobblestone_normals.png", cobbleN);
	LoadTexture(AssetPath + L"Textures/PBR/cobblestone_roughness.png", cobbleR);
	LoadTexture(AssetPath + L"Textures/PBR/cobblestone_metal.png", cobbleM);

	LoadTexture(AssetPath + L"Textures/PBR/floor_albedo.png", floorA);
	LoadTexture(AssetPath + L"Textures/PBR/floor_normals.png", floorN);
	LoadTexture(AssetPath + L"Textures/PBR/floor_roughness.png", floorR);
	LoadTexture(AssetPath + L"Textures/PBR/floor_metal.png", floorM);

	LoadTexture(AssetPath + L"Textures/PBR/paint_albedo.png", paintA);
	LoadTexture(AssetPath + L"Textures/PBR/paint_normals.png", paintN);
	LoadTexture(AssetPath + L"Textures/PBR/paint_roughness.png", paintR);
	LoadTexture(AssetPath + L"Textures/PBR/paint_metal.png", paintM);

	LoadTexture(AssetPath + L"Textures/PBR/scratched_albedo.png", scratchedA);
	LoadTexture(AssetPath + L"Textures/PBR/scratched_normals.png", scratchedN);
	LoadTexture(AssetPath + L"Textures/PBR/scratched_roughness.png", scratchedR);
	LoadTexture(AssetPath + L"Textures/PBR/scratched_metal.png", scratchedM);

	LoadTexture(AssetPath + L"Textures/PBR/bronze_albedo.png", bronzeA);
	LoadTexture(AssetPath + L"Textures/PBR/bronze_normals.png", bronzeN);
	LoadTexture(AssetPath + L"Textures/PBR/bronze_roughness.png", bronzeR);
	LoadTexture(AssetPath + L"Textures/PBR/bronze_metal.png", bronzeM);

	LoadTexture(AssetPath + L"Textures/PBR/rough_albedo.png", roughA);
	LoadTexture(AssetPath + L"Textures/PBR/rough_normals.png", roughN);
	LoadTexture(AssetPath + L"Textures/PBR/rough_roughness.png", roughR);
	LoadTexture(AssetPath + L"Textures/PBR/rough_metal.png", roughM);

	LoadTexture(AssetPath + L"Textures/PBR/wood_albedo.png", woodA);
	LoadTexture(AssetPath + L"Textures/PBR/wood_normals.png", woodN);
	LoadTexture(AssetPath + L"Textures/PBR/wood_roughness.png", woodR);
	LoadTexture(AssetPath + L"Textures/PBR/wood_metal.png", woodM);
#undef LoadTexture


	// Load shaders (some are saved for later)
	shadowVertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"ShadowVS.cso").c_str());
	solidColorPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SolidColorPS.cso").c_str());
	vertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"VertexShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> pixelShaderPBR = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShaderPBR.cso").c_str());
	std::shared_ptr<SimpleVertexShader> skyVS = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"SkyVS.cso").c_str());
	std::shared_ptr<SimplePixelShader> skyPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SkyPS.cso").c_str());

	// Load 3D models	
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>("Cube", FixPath(AssetPath + L"Meshes/cube.obj").c_str());
	std::shared_ptr<Mesh> cylinderMesh = std::make_shared<Mesh>("Cylinder", FixPath(AssetPath + L"Meshes/cylinder.obj").c_str());
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>("Helix", FixPath(AssetPath + L"Meshes/helix.obj").c_str());
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>("Sphere", FixPath(AssetPath + L"Meshes/sphere.obj").c_str());
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>("Torus", FixPath(AssetPath + L"Meshes/torus.obj").c_str());
	std::shared_ptr<Mesh> quadMesh = std::make_shared<Mesh>("Quad", FixPath(AssetPath + L"Meshes/quad.obj").c_str());
	std::shared_ptr<Mesh> quad2sidedMesh = std::make_shared<Mesh>("Double-Sided Quad", FixPath(AssetPath + L"Meshes/quad_double_sided.obj").c_str());

	// Add all meshes to vector
	meshes.insert(meshes.end(), { cubeMesh, cylinderMesh, helixMesh, sphereMesh, torusMesh, quadMesh, quad2sidedMesh });
	pointLightMesh = sphereMesh;

	// Create the sky
	sky = std::make_shared<Sky>(
		FixPath(AssetPath + L"Skies/Clouds Blue/right.png").c_str(),
		FixPath(AssetPath + L"Skies/Clouds Blue/left.png").c_str(),
		FixPath(AssetPath + L"Skies/Clouds Blue/up.png").c_str(),
		FixPath(AssetPath + L"Skies/Clouds Blue/down.png").c_str(),
		FixPath(AssetPath + L"Skies/Clouds Blue/front.png").c_str(),
		FixPath(AssetPath + L"Skies/Clouds Blue/back.png").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		sampler);



	// Create basic materials
	std::shared_ptr<Material> cobbleMat2x = std::make_shared<Material>("Cobblestone (2x Scale)", pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	cobbleMat2x->AddSampler("BasicSampler", sampler);
	cobbleMat2x->AddTextureSRV("Albedo", cobbleA);
	cobbleMat2x->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat2x->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat2x->AddTextureSRV("MetalMap", cobbleM);

	std::shared_ptr<Material> cobbleMat4x = std::make_shared<Material>("Cobblestone (4x Scale)", pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4x->AddSampler("BasicSampler", sampler);
	cobbleMat4x->AddTextureSRV("Albedo", cobbleA);
	cobbleMat4x->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat4x->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat4x->AddTextureSRV("MetalMap", cobbleM);

	std::shared_ptr<Material> floorMat = std::make_shared<Material>("Metal Floor", pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	floorMat->AddSampler("BasicSampler", sampler);
	floorMat->AddTextureSRV("Albedo", floorA);
	floorMat->AddTextureSRV("NormalMap", floorN);
	floorMat->AddTextureSRV("RoughnessMap", floorR);
	floorMat->AddTextureSRV("MetalMap", floorM);

	std::shared_ptr<Material> paintMat = std::make_shared<Material>("Blue Paint", pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	paintMat->AddSampler("BasicSampler", sampler);
	paintMat->AddTextureSRV("Albedo", paintA);
	paintMat->AddTextureSRV("NormalMap", paintN);
	paintMat->AddTextureSRV("RoughnessMap", paintR);
	paintMat->AddTextureSRV("MetalMap", paintM);

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>("Scratched Paint", pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	scratchedMat->AddSampler("BasicSampler", sampler);
	scratchedMat->AddTextureSRV("Albedo", scratchedA);
	scratchedMat->AddTextureSRV("NormalMap", scratchedN);
	scratchedMat->AddTextureSRV("RoughnessMap", scratchedR);
	scratchedMat->AddTextureSRV("MetalMap", scratchedM);

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>("Bronze", pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	bronzeMat->AddSampler("BasicSampler", sampler);
	bronzeMat->AddTextureSRV("Albedo", bronzeA);
	bronzeMat->AddTextureSRV("NormalMap", bronzeN);
	bronzeMat->AddTextureSRV("RoughnessMap", bronzeR);
	bronzeMat->AddTextureSRV("MetalMap", bronzeM);

	std::shared_ptr<Material> roughMat = std::make_shared<Material>("Rough Metal", pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	roughMat->AddSampler("BasicSampler", sampler);
	roughMat->AddTextureSRV("Albedo", roughA);
	roughMat->AddTextureSRV("NormalMap", roughN);
	roughMat->AddTextureSRV("RoughnessMap", roughR);
	roughMat->AddTextureSRV("MetalMap", roughM);

	std::shared_ptr<Material> woodMat = std::make_shared<Material>("Wood", pixelShaderPBR, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	woodMat->AddSampler("BasicSampler", sampler);
	woodMat->AddTextureSRV("Albedo", woodA);
	woodMat->AddTextureSRV("NormalMap", woodN);
	woodMat->AddTextureSRV("RoughnessMap", woodR);
	woodMat->AddTextureSRV("MetalMap", woodM);

	// Add materials to list
	materials.insert(materials.end(), { cobbleMat2x, cobbleMat4x, floorMat, paintMat, scratchedMat, bronzeMat, roughMat, woodMat });

	// === Create the scene ===
	std::shared_ptr<GameEntity> floor = std::make_shared<GameEntity>(cubeMesh, woodMat);
	floor->GetTransform()->SetScale(50, 50, 50);
	floor->GetTransform()->SetPosition(0, -52, 0);
	entities.push_back(floor);

	std::shared_ptr<GameEntity> sphere = std::make_shared<GameEntity>(sphereMesh, scratchedMat);
	sphere->GetTransform()->SetScale(2.0f);
	sphere->GetTransform()->SetPosition(-5, 0, 0);
	entities.push_back(sphere);

	std::shared_ptr<GameEntity> helix = std::make_shared<GameEntity>(helixMesh, paintMat);
	entities.push_back(helix);

	std::shared_ptr<GameEntity> cube = std::make_shared<GameEntity>(cubeMesh, cobbleMat2x);
	cube->GetTransform()->SetPosition(5, 0, 0);
	cube->GetTransform()->SetScale(2, 2, 2);
	entities.push_back(cube);

	std::shared_ptr<GameEntity> hoverSphere = std::make_shared<GameEntity>(sphereMesh, bronzeMat);
	hoverSphere->GetTransform()->SetScale(2.5f, 2.5f, 2.5f);
	hoverSphere->GetTransform()->SetPosition(0, 5, -5);
	entities.push_back(hoverSphere);
}

void Game::CreateShadowMapResources()
{
	// Reset existing API objects
	shadowOptions.ShadowDSV.Reset();
	shadowOptions.ShadowSRV.Reset();
	shadowSampler.Reset();
	shadowRasterizer.Reset();

	// Create the actual texture that will be the shadow map
	D3D11_TEXTURE2D_DESC shadowDesc = {};
	shadowDesc.Width = shadowOptions.ShadowMapResolution;
	shadowDesc.Height = shadowOptions.ShadowMapResolution;
	shadowDesc.ArraySize = 1;
	shadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	shadowDesc.CPUAccessFlags = 0;
	shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	shadowDesc.MipLevels = 1;
	shadowDesc.MiscFlags = 0;
	shadowDesc.SampleDesc.Count = 1;
	shadowDesc.SampleDesc.Quality = 0;
	shadowDesc.Usage = D3D11_USAGE_DEFAULT;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> shadowTexture;
	Graphics::Device->CreateTexture2D(&shadowDesc, 0, shadowTexture.GetAddressOf());

	// Create the depth/stencil
	D3D11_DEPTH_STENCIL_VIEW_DESC shadowDSDesc = {};
	shadowDSDesc.Format = DXGI_FORMAT_D32_FLOAT;
	shadowDSDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	shadowDSDesc.Texture2D.MipSlice = 0;
	Graphics::Device->CreateDepthStencilView(shadowTexture.Get(), &shadowDSDesc, shadowOptions.ShadowDSV.GetAddressOf());

	// Create the SRV for the shadow map
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	Graphics::Device->CreateShaderResourceView(shadowTexture.Get(), &srvDesc, shadowOptions.ShadowSRV.GetAddressOf());

	// Create the special "comparison" sampler state for shadows
	D3D11_SAMPLER_DESC shadowSampDesc = {};
	shadowSampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR; // COMPARISON filter!
	shadowSampDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
	shadowSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	shadowSampDesc.BorderColor[0] = 1.0f;
	shadowSampDesc.BorderColor[1] = 1.0f;
	shadowSampDesc.BorderColor[2] = 1.0f;
	shadowSampDesc.BorderColor[3] = 1.0f;
	Graphics::Device->CreateSamplerState(&shadowSampDesc, &shadowSampler);

	// Create a rasterizer state
	D3D11_RASTERIZER_DESC shadowRastDesc = {};
	shadowRastDesc.FillMode = D3D11_FILL_SOLID;
	shadowRastDesc.CullMode = D3D11_CULL_BACK;
	shadowRastDesc.DepthClipEnable = true;
	shadowRastDesc.DepthBias = 1000; // Multiplied by (smallest possible positive value storable in the depth buffer)
	shadowRastDesc.DepthBiasClamp = 0.0f;
	shadowRastDesc.SlopeScaledDepthBias = 1.0f;
	Graphics::Device->CreateRasterizerState(&shadowRastDesc, &shadowRasterizer);

	// Create the "camera" matrices for the shadow map rendering

	// View
	XMMATRIX shView = XMMatrixLookAtLH(
		XMVectorSet(0, 30, -30, 0),
		XMVectorSet(0, 0, 0, 0),
		XMVectorSet(0, 1, 0, 0));
	XMStoreFloat4x4(&shadowOptions.ShadowViewMatrix, shView);

	// Projection - we want ORTHOGRAPHIC for directional light shadows
	// NOTE: This particular projection is set up to be SMALLER than
	// the overall "scene", to show what happens when objects go
	// outside the shadow area.  In a game, you'd never want the
	// user to see this edge, but I'm specifically making the projection
	// small in this demo to show you that it CAN happen.
	//
	// Ideally, the first two parameters below would be adjusted to
	// fit the scene (or however much of the scene the user can see
	// at a time).  More advanced techniques, like cascaded shadow maps,
	// would use multiple (usually 4) shadow maps with increasingly larger
	// projections to ensure large open world games have shadows "everywhere"
	XMMATRIX shProj = XMMatrixOrthographicLH(shadowOptions.ShadowProjectionSize, shadowOptions.ShadowProjectionSize, 0.1f, 100.0f);
	XMStoreFloat4x4(&shadowOptions.ShadowProjectionMatrix, shProj);
}


// --------------------------------------------------------
// Creates 3 specific directional lights and many
// randomized point lights
// --------------------------------------------------------
void Game::GenerateLights()
{
	// Reset
	lights.clear();

	// Setup directional lights
	Light dir1 = {};
	dir1.Type = LIGHT_TYPE_DIRECTIONAL;
	dir1.Direction = XMFLOAT3(0, -1, 1);
	dir1.Color = XMFLOAT3(0.8f, 0.8f, 0.8f);
	dir1.Intensity = 1.0f;
	dir1.CastsShadows = 1; // 0 = false, 1 = true

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
	for (int i = 3; i < MAX_LIGHTS; i++)
	{
		Light point = {};
		point.Type = LIGHT_TYPE_POINT;
		point.Position = XMFLOAT3(RandomRange(-15.0f, 15.0f), RandomRange(-2.0f, 5.0f), RandomRange(-15.0f, 15.0f));
		point.Color = XMFLOAT3(RandomRange(0, 1), RandomRange(0, 1), RandomRange(0, 1));
		point.Range = RandomRange(5.0f, 10.0f);
		point.Intensity = RandomRange(0.1f, 3.0f);

		// Adjust either X or Z
		float lightAdjust = (float)sin(i) * 5;
		if (i % 2 == 0) point.Position.x = lightAdjust;
		else			point.Position.z = lightAdjust;

		// Add to the list
		lights.push_back(point);
	}

	// Make sure we're exactly MAX_LIGHTS big
	lights.resize(MAX_LIGHTS);
}


// --------------------------------------------------------
// Handle resizing to match the new window size
//  - Eventually, we'll want to update our 3D camera
// --------------------------------------------------------
void Game::OnResize()
{
	// Update the camera's projection to match the new aspect ratio
	if (camera) camera->UpdateProjectionMatrix(Window::AspectRatio());
}


// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Set up the new frame for the UI, then build
	// this frame's interface.  Note that the building
	// of the UI could happen at any point during update.
	UINewFrame(deltaTime);

	// Check the shadow map resolution before and after the UI.
	// If it changes, we need to recreate the shadow map
	int oldShadowRes = shadowOptions.ShadowMapResolution;
	BuildUI(camera, meshes, entities, materials, lights, lightOptions, shadowOptions);
	if (oldShadowRes != shadowOptions.ShadowMapResolution)
		CreateShadowMapResources();

	// Example input checking: Quit if the escape key is pressed
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();

	// Update the camera this frame
	camera->Update(deltaTime);

	// Updating timings
	if (!lightOptions.FreezeEntityMovement)	lightOptions.EntityMoveTime += deltaTime;
	if (!lightOptions.FreezeLightMovement)	lightOptions.LightMoveTime += deltaTime;

	// Move lights
	for (int i = 0; i < MAX_LIGHTS && !lightOptions.FreezeLightMovement; i++)
	{
		// Only adjust point lights
		if (lights[i].Type == LIGHT_TYPE_POINT)
		{
			// Adjust either X or Z
			float lightAdjust = (float)sin(lightOptions.LightMoveTime + i) * 5;

			if (i % 2 == 0) lights[i].Position.x = lightAdjust;
			else			lights[i].Position.z = lightAdjust;
		}
	}

	// First three moving entities move up and down
	float height = sin(lightOptions.EntityMoveTime) * 2.0f;
	entities[1]->GetTransform()->SetPosition(-5, height, 0);
	entities[2]->GetTransform()->SetPosition(0, height, 0);
	entities[3]->GetTransform()->SetPosition(5, height, 0);

	// Fourth moves side to side
	entities[4]->GetTransform()->SetPosition(sin(lightOptions.EntityMoveTime * 2) * 8.0f, 5, -5);
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
		// Clear the back buffer (erase what's on screen) and depth buffer
		const float color[4] = { 0, 0, 0, 0 };
		Graphics::Context->ClearRenderTargetView(Graphics::BackBufferRTV.Get(),	color);
		Graphics::Context->ClearDepthStencilView(Graphics::DepthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	// Render the shadow map before rendering anything to the screen
	RenderShadowMap();

	// DRAW geometry
	// Loop through the game entities and draw each one
	// - Note: A constant buffer has already been bound to
	//   the vertex shader stage of the pipeline (see Init above)
	for (auto& e : entities)
	{
		std::shared_ptr<SimpleVertexShader> vs = e->GetMaterial()->GetVertexShader();
		vs->SetMatrix4x4("shadowView", shadowOptions.ShadowViewMatrix);
		vs->SetMatrix4x4("shadowProjection", shadowOptions.ShadowProjectionMatrix);

		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", lightOptions.AmbientColor);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("lightCount", lightOptions.LightCount);

		ps->SetShaderResourceView("ShadowMap", shadowOptions.ShadowSRV);
		ps->SetSamplerState("ShadowSampler", shadowSampler);

		// Draw one entity
		e->Draw(camera);
	}

	// Draw the sky after all regular entities
	sky->Draw(camera);

	// Draw the light sources
	if (lightOptions.DrawLights) DrawLightSources();

	// We need to un-bind (deactivate) the shadow map as a 
	// shader resource since we'll be using it as a depth buffer
	// at the beginning of next frame!  To make it easy, I'm simply
	// unbinding all SRV's from pixel shader stage here
	ID3D11ShaderResourceView* nullSRVs[16] = {};
	Graphics::Context->PSSetShaderResources(0, 16, nullSRVs);

	// Frame END
	// - These should happen exactly ONCE PER FRAME
	// - At the very end of the frame (after drawing *everything*)
	{
		// Draw the UI after everything else
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		// Present at the end of the frame
		bool vsync = Graphics::VsyncState();
		Graphics::SwapChain->Present(
			vsync ? 1 : 0,
			vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Re-bind back buffer and depth buffer after presenting
		Graphics::Context->OMSetRenderTargets(
			1,
			Graphics::BackBufferRTV.GetAddressOf(),
			Graphics::DepthBufferDSV.Get());
	}
}


// --------------------------------------------------------
// Draws a colored sphere at the position of each point light
// --------------------------------------------------------
void Game::DrawLightSources()
{
	// Turn on these shaders
	vertexShader->SetShader();
	solidColorPS->SetShader();

	// Set up vertex shader
	vertexShader->SetMatrix4x4("view", camera->GetView());
	vertexShader->SetMatrix4x4("projection", camera->GetProjection());

	for (int i = 0; i < lightOptions.LightCount; i++)
	{
		Light light = lights[i];

		// Only drawing point lights here
		if (light.Type != LIGHT_TYPE_POINT)
			continue;

		// Calc quick scale based on range
		float scale = light.Range * light.Range / 200.0f;

		XMMATRIX scaleMat = XMMatrixScaling(scale, scale, scale);
		XMMATRIX transMat = XMMatrixTranslation(light.Position.x, light.Position.y, light.Position.z);

		// Make the transform for this light
		XMFLOAT4X4 world;
		XMStoreFloat4x4(&world, scaleMat * transMat);

		// Set up the world matrix for this light
		vertexShader->SetMatrix4x4("world", world);

		// Set up the pixel shader data
		XMFLOAT3 finalColor = light.Color;
		finalColor.x *= light.Intensity;
		finalColor.y *= light.Intensity;
		finalColor.z *= light.Intensity;
		solidColorPS->SetFloat3("Color", finalColor);

		// Copy data
		vertexShader->CopyAllBufferData();
		solidColorPS->CopyAllBufferData();

		// Draw
		pointLightMesh->SetBuffersAndDraw();
	}

}



// -------------------------------------------------------
// Renders the shadow map from the light's point of view
// -------------------------------------------------------
void Game::RenderShadowMap()
{
	// Initial pipeline setup - No RTV necessary - Clear shadow map
	Graphics::Context->OMSetRenderTargets(0, 0, shadowOptions.ShadowDSV.Get());
	Graphics::Context->ClearDepthStencilView(shadowOptions.ShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	Graphics::Context->RSSetState(shadowRasterizer.Get());

	// Need to create a viewport that matches the shadow map resolution
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)shadowOptions.ShadowMapResolution;
	viewport.Height = (float)shadowOptions.ShadowMapResolution;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	Graphics::Context->RSSetViewports(1, &viewport);

	// Turn on our shadow map Vertex Shader
	// and turn OFF the pixel shader entirely
	shadowVertexShader->SetShader();
	shadowVertexShader->SetMatrix4x4("view", shadowOptions.ShadowViewMatrix);
	shadowVertexShader->SetMatrix4x4("projection", shadowOptions.ShadowProjectionMatrix);
	Graphics::Context->PSSetShader(0, 0, 0); // No PS

	// Loop and draw all entities
	for (auto& e : entities)
	{
		shadowVertexShader->SetMatrix4x4("world", e->GetTransform()->GetWorldMatrix());
		shadowVertexShader->CopyAllBufferData();

		// Draw the mesh
		e->GetMesh()->SetBuffersAndDraw();
	}

	// If the light "bulbs" are being shown, render those to the shadow map, too
	for (int i = 0; lightOptions.DrawLights && i < lightOptions.LightCount; i++)
	{
		// Only drawing point lights here
		Light light = lights[i];
		if (light.Type != LIGHT_TYPE_POINT)
			continue;

		// Calc quick scale based on range
		float scale = light.Range * light.Range / 200.0f;
		XMMATRIX scaleMat = XMMatrixScaling(scale, scale, scale);
		XMMATRIX transMat = XMMatrixTranslation(light.Position.x, light.Position.y, light.Position.z);

		// Make the transform for this light
		XMFLOAT4X4 world;
		XMStoreFloat4x4(&world, scaleMat * transMat);
		shadowVertexShader->SetMatrix4x4("world", world);
		shadowVertexShader->CopyAllBufferData();

		pointLightMesh->SetBuffersAndDraw();
	}

	// After rendering the shadow map, go back to the screen
	Graphics::Context->OMSetRenderTargets(1, Graphics::BackBufferRTV.GetAddressOf(), Graphics::DepthBufferDSV.Get());
	viewport.Width = (float)Window::Width();
	viewport.Height = (float)Window::Height();
	Graphics::Context->RSSetViewports(1, &viewport);
	Graphics::Context->RSSetState(0);
}
