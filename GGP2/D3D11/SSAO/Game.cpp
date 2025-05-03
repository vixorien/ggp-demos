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

	// Set up the scene and create lights
	LoadAssetsAndCreateEntities();
	currentScene = &entitiesLineup;
	GenerateLights();

	// Set up defaults for lighting options
	lightOptions = {
		.LightCount = 0,
		.GammaCorrection = true,
		.UseAlbedoTexture = true,
		.UseMetalMap = true,
		.UseNormalMap = true,
		.UseRoughnessMap = true,
		.UsePBR = true,
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

	// Set up SSAO data
	{
		ssaoOptions.SSAOEnabled = true;
		ssaoOptions.SSAOOutputOnly = true;
		ssaoOptions.SampleCount = 64;
		ssaoOptions.SampleRadius = 1.0f;

		// SSAO offset vectors
		for (int i = 0; i < ARRAYSIZE(ssaoOffsets); i++)
		{
			ssaoOffsets[i] = XMFLOAT4(
				(float)rand() / RAND_MAX * 2 - 1,	// -1 to 1
				(float)rand() / RAND_MAX * 2 - 1,	// -1 to 1
				(float)rand() / RAND_MAX,			// 0 to 1
				0);

			XMVECTOR v = XMVector3Normalize(XMLoadFloat4(&ssaoOffsets[i]));

			// Scale up over the array
			float scale = (float)i / ARRAYSIZE(ssaoOffsets);
			XMVECTOR scaleVector = XMVectorLerp(
				XMVectorSet(0.1f, 0.1f, 0.1f, 1),
				XMVectorSet(1, 1, 1, 1),
				scale * scale);

			XMStoreFloat4(&ssaoOffsets[i], v * scaleVector);
		}

		// Create a random texture for SSAO
		const int textureSize = 4;
		const int totalPixels = textureSize * textureSize;
		XMFLOAT4 randomPixels[totalPixels] = {};
		for (int i = 0; i < totalPixels; i++)
		{
			XMVECTOR randomVec = XMVectorSet(RandomRange(-1, 1), RandomRange(-1, 1), 0, 0);
			XMStoreFloat4(&randomPixels[i], XMVector3Normalize(randomVec));
		}
		ssaoRandomSRV = CreateFloatTextureSRV(textureSize, textureSize, randomPixels);
	}
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

	// Also create a clamp sampler necessary for IBL sampling
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clampSampler;
	Graphics::Device->CreateSamplerState(&sampDesc, clampSampler.GetAddressOf());

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

	// Render target setup
	ResizeAllRenderTargets();


	// Load shaders (some are saved for later)
	vertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"VertexShader.cso").c_str());
	pixelShader = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShader.cso").c_str());
	pixelShaderPBR = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShaderPBR.cso").c_str());
	solidColorPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SolidColorPS.cso").c_str());
	std::shared_ptr<SimpleVertexShader> skyVS = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"SkyVS.cso").c_str());
	std::shared_ptr<SimplePixelShader> skyPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SkyPS.cso").c_str());

	fullscreenVS = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"FullscreenVS.cso").c_str());
	texturePS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SimpleTexturePS.cso").c_str());
	ssaoCalculatePS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SsaoCalculatePS.cso").c_str());
	ssaoBlurPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SsaoBlurPS.cso").c_str());
	ssaoCombinePS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SsaoCombinePS.cso").c_str());


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

	// Create the sky, but first set up IBL options (including shaders)
	iblOptions = {};
	iblOptions.IndirectLightingEnabled = true;
	iblOptions.FullscreenVS = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"FullscreenVS.cso").c_str());
	iblOptions.IBLIrradiancePS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"IBLIrradianceMapPS.cso").c_str());
	iblOptions.IBLBRDFLookUpPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"IBLBrdfLookUpTablePS.cso").c_str());
	iblOptions.IBLSpecularConvolutionPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"IBLSpecularConvolutionPS.cso").c_str());

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
		sampler,
		iblOptions);



	// Create basic materials
	std::shared_ptr<Material> cobbleMat2x = std::make_shared<Material>("Cobblestone (2x Scale)", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	cobbleMat2x->AddSampler("BasicSampler", sampler);
	cobbleMat2x->AddSampler("ClampSampler", clampSampler);
	cobbleMat2x->AddTextureSRV("Albedo", cobbleA);
	cobbleMat2x->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat2x->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat2x->AddTextureSRV("MetalMap", cobbleM);

	std::shared_ptr<Material> cobbleMat4x = std::make_shared<Material>("Cobblestone (4x Scale)", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4x->AddSampler("BasicSampler", sampler);
	cobbleMat4x->AddSampler("ClampSampler", clampSampler);
	cobbleMat4x->AddTextureSRV("Albedo", cobbleA);
	cobbleMat4x->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat4x->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat4x->AddTextureSRV("MetalMap", cobbleM);

	std::shared_ptr<Material> floorMat = std::make_shared<Material>("Metal Floor", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	floorMat->AddSampler("BasicSampler", sampler);
	floorMat->AddSampler("ClampSampler", clampSampler);
	floorMat->AddTextureSRV("Albedo", floorA);
	floorMat->AddTextureSRV("NormalMap", floorN);
	floorMat->AddTextureSRV("RoughnessMap", floorR);
	floorMat->AddTextureSRV("MetalMap", floorM);

	std::shared_ptr<Material> paintMat = std::make_shared<Material>("Blue Paint", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	paintMat->AddSampler("BasicSampler", sampler);
	paintMat->AddSampler("ClampSampler", clampSampler);
	paintMat->AddTextureSRV("Albedo", paintA);
	paintMat->AddTextureSRV("NormalMap", paintN);
	paintMat->AddTextureSRV("RoughnessMap", paintR);
	paintMat->AddTextureSRV("MetalMap", paintM);

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>("Scratched Paint", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	scratchedMat->AddSampler("BasicSampler", sampler);
	scratchedMat->AddSampler("ClampSampler", clampSampler);
	scratchedMat->AddTextureSRV("Albedo", scratchedA);
	scratchedMat->AddTextureSRV("NormalMap", scratchedN);
	scratchedMat->AddTextureSRV("RoughnessMap", scratchedR);
	scratchedMat->AddTextureSRV("MetalMap", scratchedM);

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>("Bronze", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	bronzeMat->AddSampler("BasicSampler", sampler);
	bronzeMat->AddSampler("ClampSampler", clampSampler);
	bronzeMat->AddTextureSRV("Albedo", bronzeA);
	bronzeMat->AddTextureSRV("NormalMap", bronzeN);
	bronzeMat->AddTextureSRV("RoughnessMap", bronzeR);
	bronzeMat->AddTextureSRV("MetalMap", bronzeM);

	std::shared_ptr<Material> roughMat = std::make_shared<Material>("Rough Metal", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	roughMat->AddSampler("BasicSampler", sampler);
	roughMat->AddSampler("ClampSampler", clampSampler);
	roughMat->AddTextureSRV("Albedo", roughA);
	roughMat->AddTextureSRV("NormalMap", roughN);
	roughMat->AddTextureSRV("RoughnessMap", roughR);
	roughMat->AddTextureSRV("MetalMap", roughM);

	std::shared_ptr<Material> woodMat = std::make_shared<Material>("Wood", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	woodMat->AddSampler("BasicSampler", sampler);
	woodMat->AddSampler("ClampSampler", clampSampler);
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

	// Create a pyramid of entities
	float spacing = 2.0f;
	float startY = -3.0f;
	for (int layer = 0; layer < 4; layer++)
	{
		for (int x = 0; x < layer + 1; x++)
		{
			for (int z = 0; z < layer + 1; z++)
			{
				std::shared_ptr<GameEntity> e = std::make_shared<GameEntity>(sphereMesh, woodMat);
				e->GetTransform()->SetScale(spacing * 0.5f);
				e->GetTransform()->SetPosition(
					x * spacing - (layer * 0.5f * spacing),
					startY - layer * spacing * 0.7f,
					z * spacing - (layer * 0.5f * spacing));

				entitiesLineup.push_back(e);
			}
		}
	}



	// === Create a gradient of entities based on roughness & metalness ====
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedoWhiteSRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(1, 1, 1, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedoBlackSRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(0, 0, 0, 1));
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
		matMetal->AddTextureSRV("Albedo", albedoWhiteSRV);
		matMetal->AddTextureSRV("NormalMap", normalSRV);
		matMetal->AddTextureSRV("RoughnessMap", roughSRV);
		matMetal->AddTextureSRV("MetalMap", metal1SRV);

		std::shared_ptr<Material> matNonMetal_W = std::make_shared<Material>("Non-Metal White 0-1", pixelShader, vertexShader, XMFLOAT3(1, 1, 1));
		matNonMetal_W->AddSampler("BasicSampler", sampler);
		matNonMetal_W->AddTextureSRV("Albedo", albedoWhiteSRV);
		matNonMetal_W->AddTextureSRV("NormalMap", normalSRV);
		matNonMetal_W->AddTextureSRV("RoughnessMap", roughSRV);
		matNonMetal_W->AddTextureSRV("MetalMap", metal0SRV);

		std::shared_ptr<Material> matNonMetal_B = std::make_shared<Material>("Non-Metal Black 0-1", pixelShader, vertexShader, XMFLOAT3(0, 0, 0));
		matNonMetal_B->AddSampler("BasicSampler", sampler);
		matNonMetal_B->AddTextureSRV("Albedo", albedoBlackSRV);
		matNonMetal_B->AddTextureSRV("NormalMap", normalSRV);
		matNonMetal_B->AddTextureSRV("RoughnessMap", roughSRV);
		matNonMetal_B->AddTextureSRV("MetalMap", metal0SRV);

		materials.insert(materials.end(), { matMetal, matNonMetal_W, matNonMetal_B });

		// Create the entities
		std::shared_ptr<GameEntity> geMetal = std::make_shared<GameEntity>(sphereMesh, matMetal);
		std::shared_ptr<GameEntity> geNonMetal_W = std::make_shared<GameEntity>(sphereMesh, matNonMetal_W);
		std::shared_ptr<GameEntity> geNonMetal_B = std::make_shared<GameEntity>(sphereMesh, matNonMetal_B);
		entitiesGradient.push_back(geMetal);
		entitiesGradient.push_back(geNonMetal_W);
		entitiesGradient.push_back(geNonMetal_B);

		// Move and scale them
		geMetal->GetTransform()->SetPosition(i * 2.0f - 10.0f, 2, 0);
		geNonMetal_W->GetTransform()->SetPosition(i * 2.0f - 10.0f, 0, 0);
		geNonMetal_B->GetTransform()->SetPosition(i * 2.0f - 10.0f, -2, 0);
	}

}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Game::CreateTextureSRV(int width, int height, DirectX::XMFLOAT4* pixels)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return 0;

	// Convert to ints
	unsigned char* intPixels = new unsigned char[width * height * 4];
	for (int i = 0; i < width * height * 4;)
	{
		int pixelIndex = i / 4;
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].x * 255);
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].y * 255);
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].z * 255);
		intPixels[i++] = (unsigned char)(pixels[pixelIndex].w * 255);
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
	data.pSysMem = intPixels;
	data.SysMemPitch = sizeof(unsigned char) * 4 * width;

	// Actually create it
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	Graphics::Device->CreateTexture2D(&td, &data, texture.GetAddressOf());

	// All done with these values
	delete[] intPixels;

	// Create the shader resource view for this texture and return
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	Graphics::Device->CreateShaderResourceView(texture.Get(), 0, srv.GetAddressOf());
	return srv;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Game::CreateFloatTextureSRV(int width, int height, DirectX::XMFLOAT4* pixels)
{
	// Valid size?
	if (width <= 0 || height <= 0)
		return 0;

	// Create a simple texture of the specified size
	D3D11_TEXTURE2D_DESC td = {};
	td.ArraySize = 1;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	td.MipLevels = 1;
	td.Height = height;
	td.Width = width;
	td.SampleDesc.Count = 1;

	// Initial data for the texture
	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = pixels;
	data.SysMemPitch = sizeof(float) * 4 * width;

	// Actually create it
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	Graphics::Device->CreateTexture2D(&td, &data, texture.GetAddressOf());

	// Create the shader resource view for this texture and return
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	Graphics::Device->CreateShaderResourceView(texture.Get(), 0, srv.GetAddressOf());
	return srv;
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

// ------------------------------------------------------------
// Resizes (by releasing and re-creating) the resources
// required for post processing.
// 
// We need to do this at start-up and whenever the window is resized.
// ------------------------------------------------------------
void Game::ResizeAllRenderTargets()
{
	// Resize each target (this will release and recreate it using the given format)
	ResizeRenderTarget(colorDirectRTV, colorDirectSRV, DXGI_FORMAT_R8G8B8A8_UNORM);
	ResizeRenderTarget(colorAmbientRTV, colorAmbientSRV, DXGI_FORMAT_R8G8B8A8_UNORM);
	ResizeRenderTarget(normalsRTV, normalsSRV, DXGI_FORMAT_R8G8B8A8_UNORM);
	ResizeRenderTarget(depthRTV, depthSRV, DXGI_FORMAT_R32_FLOAT);
	ResizeRenderTarget(ssaoResultsRTV, ssaoResultsSRV, DXGI_FORMAT_R8G8B8A8_UNORM);
	ResizeRenderTarget(ssaoBlurRTV, ssaoBlurSRV, DXGI_FORMAT_R8G8B8A8_UNORM);

	// Save the new SRV
	ssaoOptions.ColorDirectSRV = colorDirectSRV;
	ssaoOptions.ColorAmbientSRV = colorAmbientSRV;
	ssaoOptions.NormalsSRV = normalsSRV;
	ssaoOptions.DepthSRV = depthSRV;
	ssaoOptions.SSAOResultsSRV = ssaoResultsSRV;
	ssaoOptions.SSAOBlurSRV = ssaoBlurSRV;
}

void Game::ResizeRenderTarget(
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv,
	DXGI_FORMAT colorFormat)
{
	// Describe the render target
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = (unsigned int)(Window::Width());
	textureDesc.Height = (unsigned int)(Window::Height());
	textureDesc.ArraySize = 1;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Will render to it and sample from it!
	textureDesc.CPUAccessFlags = 0;
	textureDesc.Format = colorFormat;
	textureDesc.MipLevels = 1;
	textureDesc.MiscFlags = 0;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
	Graphics::Device->CreateTexture2D(&textureDesc, 0, tex.GetAddressOf());

	// Create the RTV and SRV using a null description which uses default options
	Graphics::Device->CreateRenderTargetView(tex.Get(), 0, rtv.ReleaseAndGetAddressOf());
	Graphics::Device->CreateShaderResourceView(tex.Get(), 0, srv.ReleaseAndGetAddressOf());
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



// --------------------------------------------------------
// Handle resizing to match the new window size
//  - Eventually, we'll want to update our 3D camera
// --------------------------------------------------------
void Game::OnResize()
{
	// Update the camera's projection to match the new aspect ratio
	if (camera) camera->UpdateProjectionMatrix(Window::AspectRatio());

	// Ensure we resize the post process resources too
	if (Graphics::Device) ResizeAllRenderTargets();
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
	BuildUI(camera, meshes, *currentScene, materials, lights, lightOptions, iblOptions, ssaoOptions);

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
	lightOptions.LightCount = max(0, min(MAX_LIGHTS, lightOptions.LightCount));
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
		const float black[4] = { 0, 0, 0, 1 };
		const float white[4] = { 1, 1, 1, 1 };
		Graphics::Context->ClearRenderTargetView(Graphics::BackBufferRTV.Get(),	black);
		Graphics::Context->ClearDepthStencilView(Graphics::DepthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
		
		// Clear all targets
		Graphics::Context->ClearRenderTargetView(colorDirectRTV.Get(), black);
		Graphics::Context->ClearRenderTargetView(colorAmbientRTV.Get(), black);
		Graphics::Context->ClearRenderTargetView(normalsRTV.Get(), black);
		Graphics::Context->ClearRenderTargetView(depthRTV.Get(), white);
		Graphics::Context->ClearRenderTargetView(ssaoResultsRTV.Get(), black);
		Graphics::Context->ClearRenderTargetView(ssaoBlurRTV.Get(), black);
	}

	// --- Pre-Draw - Swap render targets ---------------------
	{
		// Create an array of render targets
		ID3D11RenderTargetView* rtvs[4] =
		{
			colorDirectRTV.Get(),
			colorAmbientRTV.Get(),
			normalsRTV.Get(),
			depthRTV.Get()
		};

		// Change the render target to render directly into our post-process texture
		Graphics::Context->OMSetRenderTargets(4, rtvs, Graphics::DepthBufferDSV.Get());
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

		// Set up IBL options
		ps->SetInt("indirectLightingEnabled", iblOptions.IndirectLightingEnabled);
		ps->SetInt("specularIBLTotalMipLevels", sky->GetTotalSpecularIBLMipLevels());

		// Set up IBL resources
		ps->SetShaderResourceView("IrradianceIBLMap", sky->GetIrradianceIBLMap());
		ps->SetShaderResourceView("SpecularIBLMap", sky->GetSpecularIBLMap());
		ps->SetShaderResourceView("BrdfLookUpMap", sky->GetBRDFLookUpTexture());

		// Draw one entity
		e->Draw(camera);
	}

	// Draw the sky after all regular entities
	if (lightOptions.ShowSkybox) sky->Draw(camera);

	// Draw the light sources
	if (lightOptions.DrawLights) DrawLightSources();


	// --- Post-Draw ---------------------------------

	// Turn OFF vertex and index buffers since we'll be using the
	// full-screen triangle trick
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	ID3D11Buffer* nothing = 0;
	Graphics::Context->IASetIndexBuffer(0, DXGI_FORMAT_R32_UINT, 0);
	Graphics::Context->IASetVertexBuffers(0, 1, &nothing, &stride, &offset);


	// Initial SSAO Results -----------------------
	{
		// Set SSAO target
		Graphics::Context->OMSetRenderTargets(1, ssaoResultsRTV.GetAddressOf(), 0);
		
		// Set up our post process shaders
		fullscreenVS->SetShader();
		ssaoCalculatePS->SetShader();

		// Set up cbuffer data
		XMFLOAT4X4 invProj, view = camera->GetView(), proj = camera->GetProjection();
		XMStoreFloat4x4(&invProj, XMMatrixInverse(0, XMLoadFloat4x4(&proj)));
		ssaoCalculatePS->SetMatrix4x4("invProjMatrix", invProj);
		ssaoCalculatePS->SetMatrix4x4("viewMatrix", view);
		ssaoCalculatePS->SetMatrix4x4("projectionMatrix", proj);
		ssaoCalculatePS->SetData("offsets", ssaoOffsets, sizeof(XMFLOAT4) * ARRAYSIZE(ssaoOffsets));
		ssaoCalculatePS->SetFloat("ssaoRadius", ssaoOptions.SampleRadius);
		ssaoCalculatePS->SetInt("ssaoSamples", ssaoOptions.SampleCount);
		ssaoCalculatePS->SetFloat2("randomTextureScreenScale", XMFLOAT2(Window::Width() / 4.0f, Window::Height() / 4.0f));
		ssaoCalculatePS->CopyAllBufferData();

		// Bind resources
		ssaoCalculatePS->SetShaderResourceView("Normals", normalsSRV);
		ssaoCalculatePS->SetShaderResourceView("Depths", depthSRV);
		ssaoCalculatePS->SetShaderResourceView("Random", ssaoRandomSRV);

		// Perform the draw - Just a single triangle!
		Graphics::Context->Draw(3, 0);
	}

	// SSAO Blur -----------------------
	{
		// Set blur target
		Graphics::Context->OMSetRenderTargets(1, ssaoBlurRTV.GetAddressOf(), 0);

		// Set up our post process shaders
		fullscreenVS->SetShader();
		ssaoBlurPS->SetShader();

		// Set up cbuffer data
		ssaoBlurPS->SetFloat2("pixelSize", XMFLOAT2(1.0f / Window::Width(), 1.0f / Window::Height()));
		ssaoBlurPS->CopyAllBufferData();

		// Bind resources
		ssaoBlurPS->SetShaderResourceView("SSAO", ssaoResultsSRV);

		// Perform the draw - Just a single triangle!
		Graphics::Context->Draw(3, 0);
	}

	// Final Combine ----------------------
	{
		// Back buffer
		Graphics::Context->OMSetRenderTargets(1, Graphics::BackBufferRTV.GetAddressOf(), 0);

		// Set up our post process shaders
		fullscreenVS->SetShader();
		ssaoCombinePS->SetShader();

		// Set up cbuffer data
		ssaoCombinePS->SetInt("ssaoEnabled", ssaoOptions.SSAOEnabled);
		ssaoCombinePS->SetInt("ssaoOutputOnly", ssaoOptions.SSAOOutputOnly);
		ssaoCombinePS->SetFloat2("pixelSize", XMFLOAT2(1.0f / Window::Width(), 1.0f / Window::Height()));
		ssaoCombinePS->CopyAllBufferData();

		// Bind resources
		ssaoCombinePS->SetShaderResourceView("SceneColorsDirect", colorDirectSRV);
		ssaoCombinePS->SetShaderResourceView("SceneColorsIndirect", colorAmbientSRV);
		ssaoCombinePS->SetShaderResourceView("SSAOBlur", ssaoBlurSRV);

		// Perform the draw - Just a single triangle!
		Graphics::Context->Draw(3, 0);
	}


	// Final reset (including unbind of SRVs)
	ID3D11ShaderResourceView* nullSRVs[16] = {};
	Graphics::Context->PSSetShaderResources(0, 16, nullSRVs);
	Graphics::Context->OMSetRenderTargets(1, Graphics::BackBufferRTV.GetAddressOf(), 0);


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
