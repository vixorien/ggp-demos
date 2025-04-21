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
#include <algorithm>

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

	// Set up the scene and create lights
	LoadAssetsAndCreateEntities();
	GenerateLights();

	// Set up defaults for lighting options
	lightOptions = {
		.LightCount = 3,
		.FreezeLightMovement = false,
		.DrawLights = true,
		.ShowSkybox = true
	};

	// Transparency options
	transparencyOptions = {
		.TransparencyOn = true,
		.SortTransparentObjects = true,
		.RenderTransparentBackfaces = true
	};

	// Set initial graphics API state
	Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the camera
	camera = std::make_shared<FPSCamera>(
		XMFLOAT3(0.0f, 0.0f, -15.0f),	// Position
		5.0f,					// Move speed
		0.002f,					// Look speed
		XM_PIDIV4,				// Field of view
		Window::AspectRatio(),  // Aspect ratio
		0.01f,					// Near clip
		100.0f,					// Far clip
		CameraProjectionType::Perspective);
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


	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fenceA, fenceN, fenceR, fenceM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> latticeA, latticeN, latticeR, latticeM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> glassWindowA, glassWindowN, glassWindowR, glassWindowM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> glassPatternA, glassPatternN, glassPatternR, glassPatternM;

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

	LoadTexture(AssetPath + L"Textures/PBR/Transparent/fence_albedo.png", fenceA);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/fence_normals.png", fenceN);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/fence_roughness.png", fenceR);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/fence_metal.png", fenceM);

	LoadTexture(AssetPath + L"Textures/PBR/Transparent/lattice_albedo.png", latticeA);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/lattice_normals.png", latticeN);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/lattice_roughness.png", latticeR);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/lattice_metal.png", latticeM);

	LoadTexture(AssetPath + L"Textures/PBR/Transparent/glass_window_albedo.png", glassWindowA);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/glass_window_normals.png", glassWindowN);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/glass_window_roughness.png", glassWindowR);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/glass_window_metal.png", glassWindowM);

	LoadTexture(AssetPath + L"Textures/PBR/Transparent/glass_pattern_albedo.png", glassPatternA);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/glass_pattern_normals.png", glassPatternN);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/glass_pattern_roughness.png", glassPatternR);
	LoadTexture(AssetPath + L"Textures/PBR/Transparent/glass_pattern_metal.png", glassPatternM);
#undef LoadTexture


	// Load shaders (some are saved for later)
	vertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"VertexShader.cso").c_str());
	pixelShader = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShaderPBR.cso").c_str());
	solidColorPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SolidColorPS.cso").c_str());
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
	std::shared_ptr<Material> cobbleMat2x = std::make_shared<Material>("Cobblestone (2x Scale)", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	cobbleMat2x->AddSampler("BasicSampler", sampler);
	cobbleMat2x->AddTextureSRV("Albedo", cobbleA);
	cobbleMat2x->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat2x->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat2x->AddTextureSRV("MetalMap", cobbleM);

	std::shared_ptr<Material> cobbleMat4x = std::make_shared<Material>("Cobblestone (4x Scale)", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4x->AddSampler("BasicSampler", sampler);
	cobbleMat4x->AddTextureSRV("Albedo", cobbleA);
	cobbleMat4x->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat4x->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat4x->AddTextureSRV("MetalMap", cobbleM);

	std::shared_ptr<Material> floorMat = std::make_shared<Material>("Metal Floor", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	floorMat->AddSampler("BasicSampler", sampler);
	floorMat->AddTextureSRV("Albedo", floorA);
	floorMat->AddTextureSRV("NormalMap", floorN);
	floorMat->AddTextureSRV("RoughnessMap", floorR);
	floorMat->AddTextureSRV("MetalMap", floorM);

	std::shared_ptr<Material> paintMat = std::make_shared<Material>("Blue Paint", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	paintMat->AddSampler("BasicSampler", sampler);
	paintMat->AddTextureSRV("Albedo", paintA);
	paintMat->AddTextureSRV("NormalMap", paintN);
	paintMat->AddTextureSRV("RoughnessMap", paintR);
	paintMat->AddTextureSRV("MetalMap", paintM);

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>("Scratched Paint", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	scratchedMat->AddSampler("BasicSampler", sampler);
	scratchedMat->AddTextureSRV("Albedo", scratchedA);
	scratchedMat->AddTextureSRV("NormalMap", scratchedN);
	scratchedMat->AddTextureSRV("RoughnessMap", scratchedR);
	scratchedMat->AddTextureSRV("MetalMap", scratchedM);

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>("Bronze", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	bronzeMat->AddSampler("BasicSampler", sampler);
	bronzeMat->AddTextureSRV("Albedo", bronzeA);
	bronzeMat->AddTextureSRV("NormalMap", bronzeN);
	bronzeMat->AddTextureSRV("RoughnessMap", bronzeR);
	bronzeMat->AddTextureSRV("MetalMap", bronzeM);

	std::shared_ptr<Material> roughMat = std::make_shared<Material>("Rough Metal", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	roughMat->AddSampler("BasicSampler", sampler);
	roughMat->AddTextureSRV("Albedo", roughA);
	roughMat->AddTextureSRV("NormalMap", roughN);
	roughMat->AddTextureSRV("RoughnessMap", roughR);
	roughMat->AddTextureSRV("MetalMap", roughM);

	std::shared_ptr<Material> woodMat = std::make_shared<Material>("Wood", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	woodMat->AddSampler("BasicSampler", sampler);
	woodMat->AddTextureSRV("Albedo", woodA);
	woodMat->AddTextureSRV("NormalMap", woodN);
	woodMat->AddTextureSRV("RoughnessMap", woodR);
	woodMat->AddTextureSRV("MetalMap", woodM);

	// Create transparent materials
	std::shared_ptr<Material> fenceMat = std::make_shared<Material>("Fence", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 1), XMFLOAT2(0, 0), true);
	fenceMat->AddSampler("BasicSampler", sampler);
	fenceMat->AddTextureSRV("Albedo", fenceA);
	fenceMat->AddTextureSRV("NormalMap", fenceN);
	fenceMat->AddTextureSRV("RoughnessMap", fenceR);
	fenceMat->AddTextureSRV("MetalMap", fenceM);

	std::shared_ptr<Material> latticeMat = std::make_shared<Material>("Lattice", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 0.5f), XMFLOAT2(0, 0), true);
	latticeMat->AddSampler("BasicSampler", sampler);
	latticeMat->AddTextureSRV("Albedo", latticeA);
	latticeMat->AddTextureSRV("NormalMap", latticeN);
	latticeMat->AddTextureSRV("RoughnessMap", latticeR);
	latticeMat->AddTextureSRV("MetalMap", latticeM);

	std::shared_ptr<Material> glassWindowMat = std::make_shared<Material>("Glass Window", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 0.5f), XMFLOAT2(0, 0), true);
	glassWindowMat->AddSampler("BasicSampler", sampler);
	glassWindowMat->AddTextureSRV("Albedo", glassWindowA);
	glassWindowMat->AddTextureSRV("NormalMap", glassWindowN);
	glassWindowMat->AddTextureSRV("RoughnessMap", glassWindowR);
	glassWindowMat->AddTextureSRV("MetalMap", glassWindowM);

	std::shared_ptr<Material> glassPatternMat = std::make_shared<Material>("Glass Pattern", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 1), XMFLOAT2(0, 0), true);
	glassPatternMat->AddSampler("BasicSampler", sampler);
	glassPatternMat->AddTextureSRV("Albedo", glassPatternA);
	glassPatternMat->AddTextureSRV("NormalMap", glassPatternN);
	glassPatternMat->AddTextureSRV("RoughnessMap", glassPatternR);
	glassPatternMat->AddTextureSRV("MetalMap", glassPatternM);


	// Add materials to list
	materials.insert(materials.end(), { 
		cobbleMat2x, cobbleMat4x,floorMat, paintMat, scratchedMat, bronzeMat, roughMat, woodMat, 
		fenceMat, latticeMat, glassWindowMat, glassPatternMat });

	// === Create the "randomized" entities, with a static floor ===========
	std::shared_ptr<GameEntity> floor = std::make_shared<GameEntity>(cubeMesh, cobbleMat4x);
	floor->GetTransform()->SetScale(25, 25, 25);
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
		case 9: whichMat = glassWindowMat; break;
		case 10: whichMat = glassPatternMat; break;
		}

		std::shared_ptr<GameEntity> sphere = std::make_shared<GameEntity>(sphereMesh, whichMat);
		entities.push_back(sphere);
	}
	RandomizeEntities();

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
	Graphics::Device->CreateBlendState(&blendDesc, alphaBlendState.GetAddressOf());

	// Rasterizer state to render back faces
	D3D11_RASTERIZER_DESC rastDesc = {};
	rastDesc.DepthClipEnable = true;
	rastDesc.CullMode = D3D11_CULL_FRONT;
	rastDesc.FillMode = D3D11_FILL_SOLID;
	Graphics::Device->CreateRasterizerState(&rastDesc, backfaceRasterState.GetAddressOf());
}

// --------------------------------------------------------
// Programmatically creates a texture of the given size
// where all pixels are the specified color
// --------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Game::CreateSolidColorTextureSRV(int width, int height, DirectX::XMFLOAT4 color)
{
	// Create an array of the color
	unsigned char* pixels = new unsigned char[width * height * 4];
	for (int i = 0; i < width * height * 4;)
	{
		pixels[i++] = (unsigned char)(color.x * 255);
		pixels[i++] = (unsigned char)(color.y * 255);
		pixels[i++] = (unsigned char)(color.z * 255);
		pixels[i++] = (unsigned char)(color.w * 255);
	}

	// Create a simple texture of the specified size
	D3D11_TEXTURE2D_DESC td = {};
	td.ArraySize = 1;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.MipLevels = 1;
	td.Height = height;
	td.Width = width;
	td.SampleDesc.Count = 1;

	// Initial data for the texture
	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = pixels;
	data.SysMemPitch = sizeof(unsigned char) * 4 * width;

	// Actually create it
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	Graphics::Device->CreateTexture2D(&td, &data, texture.GetAddressOf());

	// All done with pixel array
	delete[] pixels;

	// Create the shader resource view for this texture and return
	// Note: Passing in a null description creates a standard
	// SRV that has access to the entire resource (all mips, if they exist)
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	Graphics::Device->CreateShaderResourceView(texture.Get(), 0, srv.GetAddressOf());
	return srv;
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
// Randomizes the position and scale of entities
// --------------------------------------------------------
void Game::RandomizeEntities()
{
	// Loop through the entities and randomize their positions and sizes
	// Skipping the first, as that's the floor
	for (int i = 1; i < entities.size(); i++)
	{
		std::shared_ptr<GameEntity> g = entities[i];

		float size = RandomRange(0.1f, 3.0f);
		g->GetTransform()->SetScale(size, size, size);
		g->GetTransform()->SetPosition(
			RandomRange(-25.0f, 25.0f),
			RandomRange(0.0f, 3.0f),
			RandomRange(-25.0f, 25.0f));
	}
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
	BuildUI(camera, meshes, entities, materials, lights, lightOptions, transparencyOptions);

	// Example input checking: Quit if the escape key is pressed
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();

	// Update the camera this frame
	camera->Update(deltaTime);

	// Move lights
	for (int i = 0; i < lightOptions.LightCount && !lightOptions.FreezeLightMovement; i++)
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

	// Check individual input
	if (Input::KeyPress(VK_TAB)) GenerateLights();

	// Handle light count changes, clamped appropriately
	if (Input::KeyDown(VK_UP)) lightOptions.LightCount++;
	if (Input::KeyDown(VK_DOWN)) lightOptions.LightCount--;
	lightOptions.LightCount = max(1, min(MAX_LIGHTS, lightOptions.LightCount));
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

	// Clear the transparent list so we can refill for this frame
	transparentSortList.clear();

	// DRAW geometry
	// Loop through the game entities and draw each one
	// - Note: A constant buffer has already been bound to
	//   the vertex shader stage of the pipeline (see Init above)
	for (auto& e : entities)
	{
		// Skip transparent entities and instead add to the sort list
		if (e->GetMaterial()->GetTransparent())
		{
			transparentSortList.push_back(e);
			continue;
		}

		// Draw this entity
		DrawOneEntity(e, totalTime);
	}

	// Draw the sky after all regular entities
	if (lightOptions.ShowSkybox) sky->Draw(camera);

	// Draw the light sources
	if (lightOptions.DrawLights) DrawLightSources();

	// Sort the transparent objects by distance to the camera
	if (transparencyOptions.SortTransparentObjects)
	{
		// Sort using a lambda function
		std::sort(
			transparentSortList.begin(),
			transparentSortList.end(),
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
		if (transparencyOptions.TransparencyOn)
			Graphics::Context->OMSetBlendState(alphaBlendState.Get(), 0, 0xFFFFFFFF);

		// Render all transparent objects
		for (auto& e : transparentSortList)
		{
			// Draw insides if necessary, flipping the normal
			if (transparencyOptions.RenderTransparentBackfaces)
			{
				Graphics::Context->RSSetState(backfaceRasterState.Get());
				DrawOneEntity(e, totalTime, true);
				Graphics::Context->RSSetState(0);
			}

			// Draw the front faces of this entity
			DrawOneEntity(e, totalTime);
		}
		// Disable transparency afterwards
		if (transparencyOptions.TransparencyOn)
			Graphics::Context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	}

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
// Draws a single entity
// 
// entity - The entity to draw
// flipNormal - Should the normal be flipped (inverted)?  This
//              is mainly used when rendering the inside of
//              an object, often with transparency
// --------------------------------------------------------
void Game::DrawOneEntity(std::shared_ptr<GameEntity> entity, float totalTime, bool flipNormal)
{
	std::shared_ptr<SimplePixelShader> ps = entity->GetMaterial()->GetPixelShader();

	// Set total time on this entity's material's pixel shader
	// Note: If the shader doesn't have this variable, nothing happens
	ps->SetFloat("time", totalTime);
	ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
	ps->SetInt("lightCount", lightOptions.LightCount);
	ps->SetInt("flipNormal", flipNormal);

	// Draw one entity
	entity->Draw(camera);
}


// --------------------------------------------------------
// Draws a colored sphere at the position of each point light
// --------------------------------------------------------
void Game::DrawLightSources()
{
	// Turn on the light mesh
	Microsoft::WRL::ComPtr<ID3D11Buffer> vb = pointLightMesh->GetVertexBuffer();
	Microsoft::WRL::ComPtr<ID3D11Buffer> ib = pointLightMesh->GetIndexBuffer();
	unsigned int indexCount = pointLightMesh->GetIndexCount();

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

		// Set buffers in the input assembler
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		Graphics::Context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
		Graphics::Context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);

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
		Graphics::Context->DrawIndexed(indexCount, 0, 0);
	}

}
