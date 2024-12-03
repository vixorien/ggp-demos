#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "UIHelpers.h"

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

	// Set up bloom options
	bloomOptions = {
		.BloomExtractType = 0,
		.CurrentBloomLevels = MaxDemoBloomLevels,
		.ShowBloomTextures = true,
		.BloomThreshold = 0.5f,
		.SeparateIntensityPerLevel = false
		// SRVs must be updated each time they are recreated
	};

	// Set initial intensities (doesn't work with above syntax)
	for (int i = 0; i < MaxDemoBloomLevels; i++)
		bloomOptions.BloomLevelIntensities[i] = 1.0f;

	// Set up the scene and create lights
	LoadAssetsAndCreateEntities();
	currentScene = &entitiesLineup;
	GenerateLights();

	// Set up defaults for lighting options
	lightOptions = {
		.LightCount = 3,
		.GammaCorrection = false,
		.UseAlbedoTexture = false,
		.UseMetalMap = false,
		.UseNormalMap = false,
		.UseRoughnessMap = false,
		.UsePBR = false,
		.FreezeLightMovement = false,
		.DrawLights = true,
		.ShowSkybox = true,
		.UseBurleyDiffuse = false,
		.AmbientColor = XMFLOAT3(0,0,0)
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

	// Quick pre-processor macro for simplifying texture loading calls below
#define LoadTexture(path, srv) CreateWICTextureFromFile(Graphics::Device.Get(), Graphics::Context.Get(), FixPath(path).c_str(), 0, srv.GetAddressOf());
	LoadTexture(L"../../../Assets/Textures/PBR/cobblestone_albedo.png", cobbleA);
	LoadTexture(L"../../../Assets/Textures/PBR/cobblestone_normals.png", cobbleN);
	LoadTexture(L"../../../Assets/Textures/PBR/cobblestone_roughness.png", cobbleR);
	LoadTexture(L"../../../Assets/Textures/PBR/cobblestone_metal.png", cobbleM);

	LoadTexture(L"../../../Assets/Textures/PBR/floor_albedo.png", floorA);
	LoadTexture(L"../../../Assets/Textures/PBR/floor_normals.png", floorN);
	LoadTexture(L"../../../Assets/Textures/PBR/floor_roughness.png", floorR);
	LoadTexture(L"../../../Assets/Textures/PBR/floor_metal.png", floorM);

	LoadTexture(L"../../../Assets/Textures/PBR/paint_albedo.png", paintA);
	LoadTexture(L"../../../Assets/Textures/PBR/paint_normals.png", paintN);
	LoadTexture(L"../../../Assets/Textures/PBR/paint_roughness.png", paintR);
	LoadTexture(L"../../../Assets/Textures/PBR/paint_metal.png", paintM);

	LoadTexture(L"../../../Assets/Textures/PBR/scratched_albedo.png", scratchedA);
	LoadTexture(L"../../../Assets/Textures/PBR/scratched_normals.png", scratchedN);
	LoadTexture(L"../../../Assets/Textures/PBR/scratched_roughness.png", scratchedR);
	LoadTexture(L"../../../Assets/Textures/PBR/scratched_metal.png", scratchedM);

	LoadTexture(L"../../../Assets/Textures/PBR/bronze_albedo.png", bronzeA);
	LoadTexture(L"../../../Assets/Textures/PBR/bronze_normals.png", bronzeN);
	LoadTexture(L"../../../Assets/Textures/PBR/bronze_roughness.png", bronzeR);
	LoadTexture(L"../../../Assets/Textures/PBR/bronze_metal.png", bronzeM);

	LoadTexture(L"../../../Assets/Textures/PBR/rough_albedo.png", roughA);
	LoadTexture(L"../../../Assets/Textures/PBR/rough_normals.png", roughN);
	LoadTexture(L"../../../Assets/Textures/PBR/rough_roughness.png", roughR);
	LoadTexture(L"../../../Assets/Textures/PBR/rough_metal.png", roughM);

	LoadTexture(L"../../../Assets/Textures/PBR/wood_albedo.png", woodA);
	LoadTexture(L"../../../Assets/Textures/PBR/wood_normals.png", woodN);
	LoadTexture(L"../../../Assets/Textures/PBR/wood_roughness.png", woodR);
	LoadTexture(L"../../../Assets/Textures/PBR/wood_metal.png", woodM);
#undef LoadTexture


	// Load shaders (some are saved for later)
	vertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"VertexShader.cso").c_str());
	pixelShader = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShader.cso").c_str());
	pixelShaderPBR = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShaderPBR.cso").c_str());
	solidColorPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SolidColorPS.cso").c_str());
	std::shared_ptr<SimpleVertexShader> skyVS = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"SkyVS.cso").c_str());
	std::shared_ptr<SimplePixelShader> skyPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SkyPS.cso").c_str());

	// Load 3D models	
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>("Cube", FixPath(L"../../../Assets/Meshes/cube.obj").c_str());
	std::shared_ptr<Mesh> cylinderMesh = std::make_shared<Mesh>("Cylinder", FixPath(L"../../../Assets/Meshes/cylinder.obj").c_str());
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>("Helix", FixPath(L"../../../Assets/Meshes/helix.obj").c_str());
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>("Sphere", FixPath(L"../../../Assets/Meshes/sphere.obj").c_str());
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>("Torus", FixPath(L"../../../Assets/Meshes/torus.obj").c_str());
	std::shared_ptr<Mesh> quadMesh = std::make_shared<Mesh>("Quad", FixPath(L"../../../Assets/Meshes/quad.obj").c_str());
	std::shared_ptr<Mesh> quad2sidedMesh = std::make_shared<Mesh>("Double-Sided Quad", FixPath(L"../../../Assets/Meshes/quad_double_sided.obj").c_str());

	// Add all meshes to vector
	meshes.insert(meshes.end(), { cubeMesh, cylinderMesh, helixMesh, sphereMesh, torusMesh, quadMesh, quad2sidedMesh });
	pointLightMesh = sphereMesh;

	// Create the sky
	sky = std::make_shared<Sky>(
		FixPath(L"../../../Assets/Skies/Night Moon/right.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/left.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/up.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/down.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/front.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/back.png").c_str(),
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

	// Add materials to list
	materials.insert(materials.end(), { cobbleMat2x, cobbleMat4x, floorMat, paintMat, scratchedMat, bronzeMat, roughMat, woodMat });

	// === Create the "randomized" entities, with a static floor ===========
	std::shared_ptr<GameEntity> floor = std::make_shared<GameEntity>(cubeMesh, cobbleMat4x);
	floor->GetTransform()->SetScale(25, 25, 25);
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

		std::shared_ptr<GameEntity> sphere = std::make_shared<GameEntity>(sphereMesh, whichMat);
		entitiesRandom.push_back(sphere);
	}
	RandomizeEntities();

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
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedoSRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(1, 1, 1, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metal0SRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(0, 0, 0, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metal1SRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(1, 1, 1, 1));

	for (int i = 0; i <= 10; i++)
	{
		// Roughness value for this entity
		float r = i / 10.0f;

		// Create textures
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughSRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(r, r, r, 1));
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(0.5f, 0.5f, 1.0f, 1));

		// Set up the materials
		std::shared_ptr<Material> matMetal = std::make_shared<Material>("Metal 0-1", pixelShader, vertexShader, XMFLOAT3(1, 1, 1));
		matMetal->AddSampler("BasicSampler", sampler);
		matMetal->AddTextureSRV("Albedo", albedoSRV);
		matMetal->AddTextureSRV("NormalMap", normalSRV);
		matMetal->AddTextureSRV("RoughnessMap", roughSRV);
		matMetal->AddTextureSRV("MetalMap", metal1SRV);

		std::shared_ptr<Material> matNonMetal = std::make_shared<Material>("Non-Metal 0-1", pixelShader, vertexShader, XMFLOAT3(1, 1, 1));
		matNonMetal->AddSampler("BasicSampler", sampler);
		matNonMetal->AddTextureSRV("Albedo", albedoSRV);
		matNonMetal->AddTextureSRV("NormalMap", normalSRV);
		matNonMetal->AddTextureSRV("RoughnessMap", roughSRV);
		matNonMetal->AddTextureSRV("MetalMap", metal0SRV);

		materials.insert(materials.end(), { matMetal, matNonMetal });

		// Create the entities
		std::shared_ptr<GameEntity> geMetal = std::make_shared<GameEntity>(sphereMesh, matMetal);
		std::shared_ptr<GameEntity> geNonMetal = std::make_shared<GameEntity>(sphereMesh, matNonMetal);
		entitiesGradient.push_back(geMetal);
		entitiesGradient.push_back(geNonMetal);

		// Move and scale them
		geMetal->GetTransform()->SetPosition(i * 2.0f - 10.0f, 1, 0);
		geNonMetal->GetTransform()->SetPosition(i * 2.0f - 10.0f, -1, 0);
	}

	// Bloom setup
	{
		// Load shaders
		bloomExtractPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"BloomExtractPS.cso").c_str());
		gaussianBlurPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"GaussianBlurPS.cso").c_str());
		bloomCombinePS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"BloomCombinePS.cso").c_str());
		fullscreenVS = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"FullscreenVS.cso").c_str());

		// Create post process resources
		ResizeAllPostProcessResources();

		// Sampler state for post processing
		D3D11_SAMPLER_DESC ppSampDesc = {};
		ppSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		ppSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		ppSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		ppSampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		ppSampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		Graphics::Device->CreateSamplerState(&ppSampDesc, ppSampler.GetAddressOf());
	}
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

// ------------------------------------------------------------
// Resizes (by releasing and re-creating) the resources
// required for post processing.
// 
// We need to do this at start-up and whenever the window is resized.
// ------------------------------------------------------------
void Game::ResizeAllPostProcessResources()
{
	ResizeOnePostProcessResource(ppRTV, ppSRV, 1.0f, DXGI_FORMAT_R16G16B16A16_FLOAT);
	ResizeOnePostProcessResource(bloomExtractRTV, bloomExtractSRV, 0.5f, DXGI_FORMAT_R16G16B16A16_FLOAT);
	bloomOptions.PostProcessSRV = ppSRV;
	bloomOptions.BloomExtractSRV = bloomExtractSRV;

	float rtScale = 0.5f;
	for (int i = 0; i < MaxDemoBloomLevels; i++)
	{
		ResizeOnePostProcessResource(blurHorizontalRTV[i], blurHorizontalSRV[i], rtScale);
		ResizeOnePostProcessResource(blurVerticalRTV[i], blurVerticalSRV[i], rtScale);
		
		bloomOptions.BlurHorizontalSRVs[i] = blurHorizontalSRV[i];
		bloomOptions.BlurVerticalSRVs[i] = blurVerticalSRV[i];

		// Each successive bloom level is half the resolution
		rtScale *= 0.5f;
	}

}


// ------------------------------------------------------------
// Resizes (by releasing and re-creating) a single post-process resource.
// 
// We need to do this at start-up and whenever the window is resized.
// ------------------------------------------------------------
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
	textureDesc.Width = (unsigned int)(Window::Width() * renderTargetScale);
	textureDesc.Height = (unsigned int)(Window::Height() * renderTargetScale);
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
	Graphics::Device->CreateTexture2D(&textureDesc, 0, ppTexture.GetAddressOf());

	// Create the Render Target View
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = textureDesc.Format;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	Graphics::Device->CreateRenderTargetView(ppTexture.Get(), &rtvDesc, rtv.ReleaseAndGetAddressOf());

	// Create the Shader Resource View using a null description
	// which gives a default SRV with access to the whole resource
	Graphics::Device->CreateShaderResourceView(ppTexture.Get(), 0, srv.ReleaseAndGetAddressOf());
}


// --------------------------------------------------------
// Handle resizing to match the new window size
//  - Eventually, we'll want to update our 3D camera
// --------------------------------------------------------
void Game::OnResize()
{
	// Update the camera's projection to match the new aspect ratio
	if (camera) camera->UpdateProjectionMatrix(Window::AspectRatio());

	// Ensure we resize the post process resources too
	if(Graphics::Device) ResizeAllPostProcessResources();
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
	BuildUI(camera, meshes, *currentScene, materials, lights, lightOptions, bloomOptions);

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

	// Check for the all On / all Off switch
	if (Input::KeyPress('O'))
	{
		// Are they all already on?
		bool allOn =
			lightOptions.GammaCorrection &&
			lightOptions.UseAlbedoTexture &&
			lightOptions.UseMetalMap &&
			lightOptions.UseNormalMap &&
			lightOptions.UseRoughnessMap &&
			lightOptions.UsePBR;

		if (allOn)
		{
			lightOptions.GammaCorrection = false;
			lightOptions.UseAlbedoTexture = false;
			lightOptions.UseMetalMap = false;
			lightOptions.UseNormalMap = false;
			lightOptions.UseRoughnessMap = false;
			lightOptions.UsePBR = false;
		}
		else
		{
			lightOptions.GammaCorrection = true;
			lightOptions.UseAlbedoTexture = true;
			lightOptions.UseMetalMap = true;
			lightOptions.UseNormalMap = true;
			lightOptions.UseRoughnessMap = true;
			lightOptions.UsePBR = true;
		}
	}

	// Check individual input
	if (Input::KeyPress(VK_TAB)) GenerateLights();
	if (Input::KeyPress('G')) lightOptions.GammaCorrection = !lightOptions.GammaCorrection;
	if (Input::KeyPress('T')) lightOptions.UseAlbedoTexture = !lightOptions.UseAlbedoTexture;
	if (Input::KeyPress('M')) lightOptions.UseMetalMap = !lightOptions.UseMetalMap;
	if (Input::KeyPress('N')) lightOptions.UseNormalMap = !lightOptions.UseNormalMap;
	if (Input::KeyPress('R')) lightOptions.UseRoughnessMap = !lightOptions.UseRoughnessMap;
	if (Input::KeyPress('F')) lightOptions.FreezeLightMovement = !lightOptions.FreezeLightMovement;
	if (Input::KeyPress('L')) lightOptions.DrawLights = !lightOptions.DrawLights;
	if (Input::KeyPress('1')) currentScene = &entitiesLineup;
	if (Input::KeyPress('2')) currentScene = &entitiesGradient;
	if (Input::KeyPress('3'))
	{
		// If we're already on this scene, randomize it
		if (currentScene == &entitiesRandom) RandomizeEntities();

		// Swap scenes
		currentScene = &entitiesRandom;
	}

	if (Input::KeyPress('P'))
	{
		lightOptions.UsePBR = !lightOptions.UsePBR;
	}

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

	// --- Post Processing - Pre-Draw ---------------------
	{
		// Clear post process target too
		const float rtClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Black
		Graphics::Context->ClearRenderTargetView(ppRTV.Get(), rtClearColor);
		Graphics::Context->ClearRenderTargetView(bloomExtractRTV.Get(), rtClearColor);

		for (int i = 0; i < MaxDemoBloomLevels; i++)
		{
			Graphics::Context->ClearRenderTargetView(blurHorizontalRTV[i].Get(), rtClearColor);
			Graphics::Context->ClearRenderTargetView(blurVerticalRTV[i].Get(), rtClearColor);
		}

		// Change the render target to the first one for bloom
		Graphics::Context->OMSetRenderTargets(1, ppRTV.GetAddressOf(), Graphics::DepthBufferDSV.Get());
	}

	// DRAW geometry
	// Loop through the game entities and draw each one
	// - Note: A constant buffer has already been bound to
	//   the vertex shader stage of the pipeline (see Init above)
	for (auto& e : *currentScene)
	{
		// For this demo, the pixel shader may change on any frame, so
		// we're just going to swap it here.  This isn't optimal but
		// it's a simply implementation for this demo.
		std::shared_ptr<SimplePixelShader> ps = lightOptions.UsePBR ? pixelShaderPBR : pixelShader;
		e->GetMaterial()->SetPixelShader(ps);

		// Set total time on this entity's material's pixel shader
		// Note: If the shader doesn't have this variable, nothing happens
		ps->SetFloat3("ambientColor", lightOptions.AmbientColor);
		ps->SetFloat("time", totalTime);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("lightCount", lightOptions.LightCount);
		ps->SetInt("gammaCorrection", (int)lightOptions.GammaCorrection);
		ps->SetInt("useAlbedoTexture", (int)lightOptions.UseAlbedoTexture);
		ps->SetInt("useMetalMap", (int)lightOptions.UseMetalMap);
		ps->SetInt("useNormalMap", (int)lightOptions.UseNormalMap);
		ps->SetInt("useRoughnessMap", (int)lightOptions.UseRoughnessMap);
		ps->SetInt("useBurleyDiffuse", (int)lightOptions.UseBurleyDiffuse);

		// Draw one entity
		e->Draw(camera);
	}

	// Draw the sky after all regular entities
	if (lightOptions.ShowSkybox) sky->Draw(camera);

	// Draw the light sources
	if (lightOptions.DrawLights) DrawLightSources();

	// --- Post processing - Post-Draw -----------------------
	{
		// Turn OFF vertex and index buffers since we'll be using the
		// full-screen triangle trick
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		ID3D11Buffer* nothing = 0;
		Graphics::Context->IASetIndexBuffer(0, DXGI_FORMAT_R32_UINT, 0);
		Graphics::Context->IASetVertexBuffers(0, 1, &nothing, &stride, &offset);

		// This is the same vertex shader used for all post processing, so set it once
		fullscreenVS->SetShader();

		// Assuming all of the post process steps have a single sampler at register 0
		Graphics::Context->PSSetSamplers(0, 1, ppSampler.GetAddressOf());

		// Handle the bloom extraction
		BloomExtract();

		// Any bloom actually happening?
		if (bloomOptions.CurrentBloomLevels >= 1)
		{
			float levelScale = 0.5f;
			SingleDirectionBlur(levelScale, XMFLOAT2(1, 0), blurHorizontalRTV[0], bloomExtractSRV); // Bloom extract is the source
			SingleDirectionBlur(levelScale, XMFLOAT2(0, 1), blurVerticalRTV[0], blurHorizontalSRV[0]);

			// Any other levels?
			for (int i = 1; i < bloomOptions.CurrentBloomLevels; i++)
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
		Graphics::Context->PSSetShaderResources(0, 16, nullSRVs);
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

// Handles extracting the "bright" pixels to a second render target
void Game::BloomExtract()
{
	// We're using a half-sized texture for bloom extract, so adjust the viewport
	D3D11_VIEWPORT vp = {};
	vp.Width = Window::Width() * 0.5f;
	vp.Height = Window::Height() * 0.5f;
	vp.MaxDepth = 1.0f;
	Graphics::Context->RSSetViewports(1, &vp);

	// Render to the BLOOM EXTRACT texture
	Graphics::Context->OMSetRenderTargets(1, bloomExtractRTV.GetAddressOf(), 0);

	// Activate the shader and set resources
	bloomExtractPS->SetShader();
	bloomExtractPS->SetShaderResourceView("pixels", ppSRV.Get()); // IMPORTANT: This step takes the original post process texture!
	// Note: Sampler set already!

	// Set post process specific data
	bloomExtractPS->SetInt("extractType", bloomOptions.BloomExtractType);
	bloomExtractPS->SetFloat("bloomThreshold", bloomOptions.BloomThreshold);
	bloomExtractPS->CopyAllBufferData();

	// Draw exactly 3 vertices for our "full screen triangle"
	Graphics::Context->Draw(3, 0);
}


// Blurs in a single direction, based on the "blurDirection" parameter
// This allows us to use a single shader for both horizontal and vertical
// blurring, rather than having to write two nearly-identical shaders
void Game::SingleDirectionBlur(float renderTargetScale, DirectX::XMFLOAT2 blurDirection, Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sourceTexture)
{
	// Ensure our viewport matches our render target
	D3D11_VIEWPORT vp = {};
	vp.Width = Window::Width() * renderTargetScale;
	vp.Height = Window::Height() * renderTargetScale;
	vp.MaxDepth = 1.0f;
	Graphics::Context->RSSetViewports(1, &vp);

	// Target to which we're rendering
	Graphics::Context->OMSetRenderTargets(1, target.GetAddressOf(), 0);

	// Activate the shader and set resources
	gaussianBlurPS->SetShader();
	gaussianBlurPS->SetShaderResourceView("pixels", sourceTexture.Get()); // The texture from the previous step
	// Note: Sampler set already!

	// Set post process specific data
	gaussianBlurPS->SetFloat2("pixelUVSize", XMFLOAT2(1.0f / (Window::Width() * renderTargetScale), 1.0f / (Window::Height() * renderTargetScale)));
	gaussianBlurPS->SetFloat2("blurDirection", blurDirection);
	gaussianBlurPS->CopyAllBufferData();

	// Draw exactly 3 vertices for our "full screen triangle"
	Graphics::Context->Draw(3, 0);
}

// Combines all bloom levels with the original post process target
// Note: If a level isn't being used, it's still cleared to black
//       so it won't have any impact on the final result
void Game::BloomCombine()
{
	// Back to the full window viewport
	D3D11_VIEWPORT vp = {};
	vp.Width = (float)Window::Width();
	vp.Height = (float)Window::Height();
	vp.MaxDepth = 1.0f;
	Graphics::Context->RSSetViewports(1, &vp);

	// Render to the BACK BUFFER (since this is the last step!)
	Graphics::Context->OMSetRenderTargets(1, Graphics::BackBufferRTV.GetAddressOf(), 0);

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
	bloomCombinePS->SetFloat("intensityLevel0", bloomOptions.BloomLevelIntensities[0]);
	bloomCombinePS->SetFloat("intensityLevel1", bloomOptions.BloomLevelIntensities[1]);
	bloomCombinePS->SetFloat("intensityLevel2", bloomOptions.BloomLevelIntensities[2]);
	bloomCombinePS->SetFloat("intensityLevel3", bloomOptions.BloomLevelIntensities[3]);
	bloomCombinePS->SetFloat("intensityLevel4", bloomOptions.BloomLevelIntensities[4]);
	bloomCombinePS->CopyAllBufferData();

	// Draw exactly 3 vertices for our "full screen triangle"
	Graphics::Context->Draw(3, 0);
}