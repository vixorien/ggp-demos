#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Helpers.h"

#include "WICTextureLoader.h"

#include "../Common/ImGui/imgui.h"
#include "../Common/ImGui/imgui_impl_dx11.h"
#include "../Common/ImGui/imgui_impl_win32.h"

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
	lightCount(3),
	showSkybox(true),
	showUIDemoWindow(false)
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

	// ImGui clean up
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Initialize ImGui itself & platform/renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device.Get(), context.Get());
	ImGui::StyleColorsDark();

	// Seed random
	srand((unsigned int)time(0));

	// Loading scene stuff and Set the current scene 
	// (which of the 3 lists of entities are we drawing)
	LoadAssetsAndCreateEntities();
	currentScene = &entitiesLineup;

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
	// Load 3D models	
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/cube.obj").c_str(), device);
	std::shared_ptr<Mesh> cylinderMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/cylinder.obj").c_str(), device);
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/helix.obj").c_str(), device);
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/sphere.obj").c_str(), device);
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/torus.obj").c_str(), device);
	std::shared_ptr<Mesh> quadMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/quad.obj").c_str(), device);
	std::shared_ptr<Mesh> quad2sidedMesh = std::make_shared<Mesh>(FixPath(L"../../../Assets/Models/quad_double_sided.obj").c_str(), device);
	
	// Add all meshes to vector
	meshes.insert(meshes.end(), { cubeMesh, cylinderMesh, helixMesh, sphereMesh, torusMesh, quadMesh, quad2sidedMesh });

	// Use sphere when drawing light sources
	lightMesh = sphereMesh;

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

	// Declare the textures we'll need
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobbleA, cobbleN, cobbleR, cobbleM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorA, floorN, floorR, floorM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> paintA, paintN, paintR, paintM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scratchedA, scratchedN, scratchedR, scratchedM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bronzeA, bronzeN, bronzeR, bronzeM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> roughA, roughN, roughR, roughM;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodA, woodN, woodR, woodM;

// Quick pre-processor macro for simplifying texture loading calls below
#define LoadTexture(path, srv) CreateWICTextureFromFile(device.Get(), context.Get(), FixPath(path).c_str(), 0, srv.GetAddressOf());

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

	// Create the sky (loading custom shaders in-line below)
	sky = std::make_shared<Sky>(
		FixPath(L"../../../Assets/Skies/Night Moon/right.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/left.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/up.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/down.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/front.png").c_str(),
		FixPath(L"../../../Assets/Skies/Night Moon/back.png").c_str(),
		cubeMesh,
		std::make_shared<SimpleVertexShader>(device, context, FixPath(L"SkyVS.cso").c_str()),
		std::make_shared<SimplePixelShader>(device, context, FixPath(L"SkyPS.cso").c_str()),
		sampler,
		device,
		context);


	// Load shaders
	vertexShader = std::make_shared<SimpleVertexShader>(device, context, FixPath(L"VertexShader.cso").c_str());
	solidColorPS = std::make_shared<SimplePixelShader>(device, context, FixPath(L"SolidColorPS.cso").c_str());
	pixelShader = std::make_shared<SimplePixelShader>(device, context, FixPath(L"PixelShader.cso").c_str());
	pixelShaderPBR = std::make_shared<SimplePixelShader>(device, context, FixPath(L"PixelShaderPBR.cso").c_str());

	

	// Create basic materials
	std::shared_ptr<Material> cobbleMat2x = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	cobbleMat2x->AddSampler("BasicSampler", sampler);
	cobbleMat2x->AddTextureSRV("Albedo", cobbleA);
	cobbleMat2x->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat2x->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat2x->AddTextureSRV("MetalMap", cobbleM);
	
	std::shared_ptr<Material> cobbleMat4x = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4x->AddSampler("BasicSampler", sampler);
	cobbleMat4x->AddTextureSRV("Albedo", cobbleA);
	cobbleMat4x->AddTextureSRV("NormalMap", cobbleN);
	cobbleMat4x->AddTextureSRV("RoughnessMap", cobbleR);
	cobbleMat4x->AddTextureSRV("MetalMap", cobbleM);

	std::shared_ptr<Material> floorMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	floorMat->AddSampler("BasicSampler", sampler);
	floorMat->AddTextureSRV("Albedo", floorA);
	floorMat->AddTextureSRV("NormalMap", floorN);
	floorMat->AddTextureSRV("RoughnessMap", floorR);
	floorMat->AddTextureSRV("MetalMap", floorM);

	std::shared_ptr<Material> paintMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	paintMat->AddSampler("BasicSampler", sampler);
	paintMat->AddTextureSRV("Albedo", paintA);
	paintMat->AddTextureSRV("NormalMap", paintN);
	paintMat->AddTextureSRV("RoughnessMap", paintR);
	paintMat->AddTextureSRV("MetalMap", paintM);

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	scratchedMat->AddSampler("BasicSampler", sampler);
	scratchedMat->AddTextureSRV("Albedo", scratchedA);
	scratchedMat->AddTextureSRV("NormalMap", scratchedN);
	scratchedMat->AddTextureSRV("RoughnessMap", scratchedR);
	scratchedMat->AddTextureSRV("MetalMap", scratchedM);

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	bronzeMat->AddSampler("BasicSampler", sampler);
	bronzeMat->AddTextureSRV("Albedo", bronzeA);
	bronzeMat->AddTextureSRV("NormalMap", bronzeN);
	bronzeMat->AddTextureSRV("RoughnessMap", bronzeR);
	bronzeMat->AddTextureSRV("MetalMap", bronzeM);

	std::shared_ptr<Material> roughMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	roughMat->AddSampler("BasicSampler", sampler);
	roughMat->AddTextureSRV("Albedo", roughA);
	roughMat->AddTextureSRV("NormalMap", roughN);
	roughMat->AddTextureSRV("RoughnessMap", roughR);
	roughMat->AddTextureSRV("MetalMap", roughM);

	std::shared_ptr<Material> woodMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	woodMat->AddSampler("BasicSampler", sampler);
	woodMat->AddTextureSRV("Albedo", woodA);
	woodMat->AddTextureSRV("NormalMap", woodN);
	woodMat->AddTextureSRV("RoughnessMap", woodR);
	woodMat->AddTextureSRV("MetalMap", woodM);

	// Add materials to list
	materials.insert(materials.end(), { cobbleMat2x, cobbleMat4x, floorMat, paintMat, scratchedMat, bronzeMat, roughMat, woodMat });

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

		std::shared_ptr<GameEntity> sphere = std::make_shared<GameEntity>(sphereMesh, whichMat);
		entitiesRandom.push_back(sphere);
	}
	RandomizeEntities();

	// === Create the line up entities =====================================
	std::shared_ptr<GameEntity> cobSphere = std::make_shared<GameEntity>(sphereMesh, cobbleMat2x);
	cobSphere->GetTransform()->SetPosition(-6, 0, 0);
	cobSphere->GetTransform()->SetScale(2);

	std::shared_ptr<GameEntity> floorSphere = std::make_shared<GameEntity>(sphereMesh, floorMat);
	floorSphere->GetTransform()->SetPosition(-4, 0, 0);
	floorSphere->GetTransform()->SetScale(2);

	std::shared_ptr<GameEntity> paintSphere = std::make_shared<GameEntity>(sphereMesh, paintMat);
	paintSphere->GetTransform()->SetPosition(-2, 0, 0);
	paintSphere->GetTransform()->SetScale(2);

	std::shared_ptr<GameEntity> scratchSphere = std::make_shared<GameEntity>(sphereMesh, scratchedMat);
	scratchSphere->GetTransform()->SetPosition(0, 0, 0);
	scratchSphere->GetTransform()->SetScale(2);

	std::shared_ptr<GameEntity> bronzeSphere = std::make_shared<GameEntity>(sphereMesh, bronzeMat);
	bronzeSphere->GetTransform()->SetPosition(2, 0, 0);
	bronzeSphere->GetTransform()->SetScale(2);

	std::shared_ptr<GameEntity> roughSphere = std::make_shared<GameEntity>(sphereMesh, roughMat);
	roughSphere->GetTransform()->SetPosition(4, 0, 0);
	roughSphere->GetTransform()->SetScale(2);

	std::shared_ptr<GameEntity> woodSphere = std::make_shared<GameEntity>(sphereMesh, woodMat);
	woodSphere->GetTransform()->SetPosition(6, 0, 0);
	woodSphere->GetTransform()->SetScale(2);

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
		std::shared_ptr<Material> matMetal = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1));
		matMetal->AddSampler("BasicSampler", sampler);
		matMetal->AddTextureSRV("Albedo", albedoSRV);
		matMetal->AddTextureSRV("NormalMap", normalSRV);
		matMetal->AddTextureSRV("RoughnessMap", roughSRV);
		matMetal->AddTextureSRV("MetalMap", metal1SRV);

		std::shared_ptr<Material> matNonMetal = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1));
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

		geMetal->GetTransform()->SetScale(2);
		geNonMetal->GetTransform()->SetScale(2);
	}
}

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
	device->CreateTexture2D(&td, &data, texture.GetAddressOf());

	// All done with pixel array
	delete[] pixels;

	// Create the shader resource view for this texture and return
	// Note: Passing in a null description creates a standard
	// SRV that has access to the entire resource (all mips, if they exist)
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	device->CreateShaderResourceView(texture.Get(), 0, srv.GetAddressOf());
	return srv;
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

		float size = 2.0f * RandomRange(0.1f, 3.0f);
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
	// Set up the new frame for the UI, then build
	// this frame's interface.  Note that the building
	// of the UI could happen at any point during update.
	UINewFrame(deltaTime);
	BuildUI();

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


	// Loop through the game entities in the current scene and draw
	for (auto& e : *currentScene)
	{
		// For this demo, the pixel shader may change on any frame, so
		// we're just going to swap it here.  This isn't optimal but
		// it's a simply implementation for this demo.
		std::shared_ptr<SimplePixelShader> ps = usePBR ? pixelShaderPBR : pixelShader;
		e->GetMaterial()->SetPixelShader(ps);

		// Set all values
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
	if(showSkybox) sky->Draw(camera);

	// Draw the light sources
	if(drawLights) DrawLightSources();

	// Frame END
	// - These should happen exactly ONCE PER FRAME
	// - At the very end of the frame (after drawing *everything*)
	{
		// Draw the UI after everything else
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

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
	// Turn on the light mesh
	Microsoft::WRL::ComPtr<ID3D11Buffer> vb = lightMesh->GetVertexBuffer();
	Microsoft::WRL::ComPtr<ID3D11Buffer> ib = lightMesh->GetIndexBuffer();
	unsigned int indexCount = lightMesh->GetIndexCount();

	// Turn on these shaders
	vertexShader->SetShader();
	solidColorPS->SetShader();

	// Set up vertex shader
	vertexShader->SetMatrix4x4("view", camera->GetView());
	vertexShader->SetMatrix4x4("projection", camera->GetProjection());

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
		context->DrawIndexed(indexCount, 0, 0);
	}

}

// --------------------------------------------------------
// Prepares a new frame for the UI, feeding it fresh
// input and time information for this new frame.
// --------------------------------------------------------
void Game::UINewFrame(float deltaTime)
{
	// Feed fresh input data to ImGui
	ImGuiIO& io = ImGui::GetIO();
	io.DeltaTime = deltaTime;
	io.DisplaySize.x = (float)this->windowWidth;
	io.DisplaySize.y = (float)this->windowHeight;

	// Reset the frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Determine new input capture
	Input& input = Input::GetInstance();
	input.SetKeyboardCapture(io.WantCaptureKeyboard);
	input.SetMouseCapture(io.WantCaptureMouse);
}


// --------------------------------------------------------
// Builds the UI for the current frame
// --------------------------------------------------------
void Game::BuildUI()
{
	// Should we show the built-in demo window?
	if (showUIDemoWindow)
	{
		ImGui::ShowDemoWindow();
	}

	// Actually build our custom UI, starting with a window
	ImGui::Begin("Inspector");
	{
		// Set a specific amount of space for widget labels
		ImGui::PushItemWidth(-160); // Negative value sets label width

		// === Overall details ===
		if (ImGui::TreeNode("App Details"))
		{
			ImGui::Spacing();
			ImGui::Text("Frame rate: %f fps", ImGui::GetIO().Framerate);
			ImGui::Text("Window Client Size: %dx%d", windowWidth, windowHeight);

			// Should we show the demo window?
			if (ImGui::Button(showUIDemoWindow ? "Hide ImGui Demo Window" : "Show ImGui Demo Window"))
				showUIDemoWindow = !showUIDemoWindow;

			ImGui::Spacing();

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Controls ===
		if (ImGui::TreeNode("Controls"))
		{
			ImGui::Spacing();
			ImGui::Text("(WASD, X, Space)");    ImGui::SameLine(175); ImGui::Text("Move camera");
			ImGui::Text("(Left Click & Drag)"); ImGui::SameLine(175); ImGui::Text("Rotate camera");
			ImGui::Text("(Left Shift)");        ImGui::SameLine(175); ImGui::Text("Hold to speed up camera");
			ImGui::Text("(Left Ctrl)");         ImGui::SameLine(175); ImGui::Text("Hold to slow down camera");

			ImGui::Spacing();
			ImGui::Text("(Arrow Up/Down)");		ImGui::SameLine(175); ImGui::Text("Adjust light count");
			ImGui::Text("(Tab)");				ImGui::SameLine(175); ImGui::Text("Randomize lights");
			ImGui::Text("(F)");					ImGui::SameLine(175); ImGui::Text("Freeze/unfreeze lights");
			ImGui::Text("(L)");					ImGui::SameLine(175); ImGui::Text("Show/hide point lights");

			ImGui::Spacing();
			ImGui::Text("(G)");				ImGui::SameLine(175); ImGui::Text("Gamma correction");
			ImGui::Text("(P)");				ImGui::SameLine(175); ImGui::Text("PBR");
			ImGui::Text("(T)");				ImGui::SameLine(175); ImGui::Text("Albedo texture");
			ImGui::Text("(N)");				ImGui::SameLine(175); ImGui::Text("Normal map");
			ImGui::Text("(R)");				ImGui::SameLine(175); ImGui::Text("Roughness map");
			ImGui::Text("(M)");				ImGui::SameLine(175); ImGui::Text("Metalness map");
			ImGui::Text("(O)");				ImGui::SameLine(175); ImGui::Text("All material options on/off");

			ImGui::Spacing();
			ImGui::Text("(1, 2, 3)");			ImGui::SameLine(175); ImGui::Text("Change scene");


			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Camera details ===
		if (ImGui::TreeNode("Camera"))
		{
			// Show UI for current camera
			CameraUI(camera);

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Meshes ===
		if (ImGui::TreeNode("Meshes"))
		{
			// Loop and show the details for each mesh
			for (int i = 0; i < meshes.size(); i++)
			{
				ImGui::Text("Mesh %d: %d indices", i, meshes[i]->GetIndexCount());
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Entities ===
		if (ImGui::TreeNode("Scene Entities"))
		{
			ImGui::Text("Choose Scene:");
			if (ImGui::RadioButton("Material Showcase", currentScene == &entitiesLineup)) { currentScene = &entitiesLineup; }
			if (ImGui::RadioButton("Gradient Spheres", currentScene == &entitiesGradient)) { currentScene = &entitiesGradient; }
			if (ImGui::RadioButton("Random Spheres", currentScene == &entitiesRandom)) { currentScene = &entitiesRandom; }
			if (currentScene == &entitiesRandom && ImGui::Button("Randomize Entities"))
			{
				RandomizeEntities();
			}

			ImGui::Spacing();
			ImGui::Checkbox("Show Skybox", &showSkybox);

			// Loop and show the details for each entity
			ImGui::Spacing();
			for (int i = 0; i < currentScene->size(); i++)
			{
				// New node for each entity
				// Note the use of PushID(), so that each tree node and its widgets
				// have unique internal IDs in the ImGui system
				ImGui::PushID(i);
				if (ImGui::TreeNode("Entity Node", "Entity %d", i))
				{
					// Build UI for one entity at a time
					EntityUI((*currentScene)[i]);

					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Materials ===
		if (ImGui::TreeNode("Materials"))
		{
			if (ImGui::TreeNode("Global Material Controls"))
			{
				ImGui::Checkbox("Gamma Correction", &gammaCorrection);
				ImGui::Checkbox("Use PBR Materials", &usePBR);
				ImGui::Checkbox("Albedo Texture", &useAlbedoTexture);
				ImGui::Checkbox("Normal Map", &useNormalMap);
				ImGui::Checkbox("Roughness Map", &useRoughnessMap);
				ImGui::Checkbox("Metalness Map", &useMetalMap);
				if (ImGui::Button("Toggle All"))
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

				ImGui::TreePop();
				ImGui::Spacing();
			}
			
			// Loop and show the details for each entity
			for (int i = 0; i < materials.size(); i++)
			{
				// New node for each material
				// Note the use of PushID(), so that each tree node and its widgets
				// have unique internal IDs in the ImGui system
				ImGui::PushID(i);
				if (ImGui::TreeNode("Material Node", "Material %d", i))
				{
					// Build UI for one material at a time
					MaterialUI(materials[i]);

					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Lights ===
		if (ImGui::TreeNode("Lights"))
		{
			// Light details
			ImGui::Spacing();
			ImGui::ColorEdit3("Ambient Color", &ambientColor.x);
			ImGui::Checkbox("Show Point Lights", &drawLights);
			ImGui::Checkbox("Freeze Lights", &freezeLightMovement);
			ImGui::SliderInt("Light Count", &lightCount, 1, MAX_LIGHTS);
			if (ImGui::Button("Randomize Point Lights")) GenerateLights();
			ImGui::Spacing();

			// Loop and show the details for each entity
			for (int i = 0; i < lights.size(); i++)
			{
				// Name of this light based on type
				std::string lightName = "Light %d";
				switch (lights[i].Type)
				{
				case LIGHT_TYPE_DIRECTIONAL: lightName += " (Directional)"; break;
				case LIGHT_TYPE_POINT: lightName += " (Point)"; break;
				case LIGHT_TYPE_SPOT: lightName += " (Spot)"; break;
				}

				// New node for each light
				// Note the use of PushID(), so that each tree node and its widgets
				// have unique internal IDs in the ImGui system
				ImGui::PushID(i);
				if (ImGui::TreeNode("Light Node", lightName.c_str(), i))
				{
					// Build UI for one entity at a time
					LightUI(lights[i]);

					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Finalize the tree node
			ImGui::TreePop();
		}
	}
	ImGui::End();
}


// --------------------------------------------------------
// Builds the UI for a single camera
// --------------------------------------------------------
void Game::CameraUI(std::shared_ptr<Camera> cam)
{
	ImGui::Spacing();

	// Transform details
	XMFLOAT3 pos = cam->GetTransform()->GetPosition();
	XMFLOAT3 rot = cam->GetTransform()->GetPitchYawRoll();

	if (ImGui::DragFloat3("Position", &pos.x, 0.01f))
		cam->GetTransform()->SetPosition(pos);
	if (ImGui::DragFloat3("Rotation (Radians)", &rot.x, 0.01f))
		cam->GetTransform()->SetRotation(rot);
	ImGui::Spacing();

	// Clip planes
	float nearClip = cam->GetNearClip();
	float farClip = cam->GetFarClip();
	if (ImGui::DragFloat("Near Clip Distance", &nearClip, 0.01f, 0.001f, 1.0f))
		cam->SetNearClip(nearClip);
	if (ImGui::DragFloat("Far Clip Distance", &farClip, 1.0f, 10.0f, 1000.0f))
		cam->SetFarClip(farClip);

	// Projection type
	CameraProjectionType projType = cam->GetProjectionType();
	int typeIndex = (int)projType;
	if (ImGui::Combo("Projection Type", &typeIndex, "Perspective\0Orthographic"))
	{
		projType = (CameraProjectionType)typeIndex;
		cam->SetProjectionType(projType);
	}

	// Projection details
	if (projType == CameraProjectionType::Perspective)
	{
		// Convert field of view to degrees for UI
		float fov = cam->GetFieldOfView() * 180.0f / XM_PI;
		if (ImGui::SliderFloat("Field of View (Degrees)", &fov, 0.01f, 180.0f))
			cam->SetFieldOfView(fov * XM_PI / 180.0f); // Back to radians
	}
	else if (projType == CameraProjectionType::Orthographic)
	{
		float wid = cam->GetOrthographicWidth();
		if (ImGui::SliderFloat("Orthographic Width", &wid, 1.0f, 10.0f))
			cam->SetOrthographicWidth(wid);
	}

	ImGui::Spacing();
}


// --------------------------------------------------------
// Builds the UI for a single entity
// --------------------------------------------------------
void Game::EntityUI(std::shared_ptr<GameEntity> entity)
{
	ImGui::Spacing();

	// Transform details
	Transform* trans = entity->GetTransform();
	XMFLOAT3 pos = trans->GetPosition();
	XMFLOAT3 rot = trans->GetPitchYawRoll();
	XMFLOAT3 sca = trans->GetScale();

	if (ImGui::DragFloat3("Position", &pos.x, 0.01f)) trans->SetPosition(pos);
	if (ImGui::DragFloat3("Rotation (Radians)", &rot.x, 0.01f)) trans->SetRotation(rot);
	if (ImGui::DragFloat3("Scale", &sca.x, 0.01f)) trans->SetScale(sca);

	// Mesh details
	ImGui::Spacing();
	ImGui::Text("Mesh Index Count: %d", entity->GetMesh()->GetIndexCount());

	ImGui::Spacing();
}


// --------------------------------------------------------
// Builds the UI for a single material
// --------------------------------------------------------
void Game::MaterialUI(std::shared_ptr<Material> material)
{
	ImGui::Spacing();

	// Color tint editing
	XMFLOAT3 tint = material->GetColorTint();
	if (ImGui::ColorEdit3("Color Tint", &tint.x))
		material->SetColorTint(tint);

	ImGui::Spacing();
}

// --------------------------------------------------------
// Builds the UI for a single light
// --------------------------------------------------------
void Game::LightUI(Light& light)
{
	// Light type
	if (ImGui::RadioButton("Directional", light.Type == LIGHT_TYPE_DIRECTIONAL))
	{
		light.Type = LIGHT_TYPE_DIRECTIONAL;
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Point", light.Type == LIGHT_TYPE_POINT))
	{
		light.Type = LIGHT_TYPE_POINT;
	}
	ImGui::SameLine();

	if (ImGui::RadioButton("Spot", light.Type == LIGHT_TYPE_SPOT))
	{
		light.Type = LIGHT_TYPE_SPOT;
	}

	// Direction
	if (light.Type == LIGHT_TYPE_DIRECTIONAL || light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::DragFloat3("Direction", &light.Direction.x, 0.1f);

		// Normalize the direction
		XMVECTOR dirNorm = XMVector3Normalize(XMLoadFloat3(&light.Direction));
		XMStoreFloat3(&light.Direction, dirNorm);
	}

	// Position & Range
	if (light.Type == LIGHT_TYPE_POINT || light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::DragFloat3("Position", &light.Position.x, 0.1f);
		ImGui::SliderFloat("Range", &light.Range, 0.1f, 100.0f);
	}

	// Spot falloff
	if (light.Type == LIGHT_TYPE_SPOT)
	{
		ImGui::SliderFloat("Spot Falloff", &light.SpotFalloff, 0.1f, 128.0f);
	}

	// Color details
	ImGui::ColorEdit3("Color", &light.Color.x);
	ImGui::SliderFloat("Intensity", &light.Intensity, 0.0f, 10.0f);
}
