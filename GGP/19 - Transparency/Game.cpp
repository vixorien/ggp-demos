#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "UIHelpers.h"
#include "AssetPath.h"
#include "BufferStructs.h"

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
	randomEntityCount = 32;
	LoadAssetsAndCreateEntities();
	GenerateLights();

	// Set up defaults for lighting options
	lightOptions = {
		.LightCount = 4,
		.FreezeLightMovement = false,
		.DrawLights = true,
		.ShowSkybox = true
	};

	// Transparency options
	transparencyOptions = {
		.TransparencyOn = true,
		.AlphaClippingOn = true,
		.SortTransparentObjects = true,
		.RenderTransparentBackfaces = true
	};

	// Set initial graphics API state
	//  - These settings persist until we change them
	//  - Some of these, like the primitive topology & input layout, probably won't change
	//  - Others, like setting shaders, will need to be moved elsewhere later
	{
		// Set up a constant buffer heap of an appropriate size
		Graphics::ResizeConstantBufferHeap(256 * 5000); // 5000 chunks of 256 bytes

		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Create an input layout 
		//  - This describes the layout of data sent to a vertex shader
		//  - In other words, it describes how to interpret data (numbers) in a vertex buffer
		//  - Doing this NOW because it requires a vertex shader's byte code to verify against!
		D3D11_INPUT_ELEMENT_DESC inputElements[4] = {};

		// Set up the first element - a position, which is 3 float values
		inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;				// Most formats are described as color channels; really it just means "Three 32-bit floats"
		inputElements[0].SemanticName = "POSITION";							// This is "POSITION" - needs to match the semantics in our vertex shader input!
		inputElements[0].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;	// How far into the vertex is this?  Assume it's after the previous element

		// Set up the second element - a uv, which is 2 more float values
		inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;					// 2x 32-bit floats
		inputElements[1].SemanticName = "TEXCOORD";							// Match our vertex shader input!
		inputElements[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;	// After the previous element

		// Set up the third element - a normal, which is 3 more float values
		inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;				// 3x 32-bit floats
		inputElements[2].SemanticName = "NORMAL";							// Match our vertex shader input!
		inputElements[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;	// After the previous element

		// Set up the fourth element - a tangent, which is 3 more float values
		inputElements[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;				// 3x 32-bit floats
		inputElements[3].SemanticName = "TANGENT";							// Match our vertex shader input!
		inputElements[3].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;	// After the previous element


		// Create the input layout, verifying our description against actual shader code
		ID3DBlob* vertexShaderBlob;
		D3DReadFileToBlob(FixPath(L"VertexShader.cso").c_str(), &vertexShaderBlob);
		Graphics::Device->CreateInputLayout(
			inputElements,							// An array of descriptions
			ARRAYSIZE(inputElements),				// How many elements in that array?
			vertexShaderBlob->GetBufferPointer(),	// Pointer to the code of a shader that uses this layout
			vertexShaderBlob->GetBufferSize(),		// Size of the shader code that uses this layout
			inputLayout.GetAddressOf());			// Address of the resulting ID3D11InputLayout pointer

		// Set the input layout now that it exists
		Graphics::Context->IASetInputLayout(inputLayout.Get());
	}

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


	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> leafA, leafN, leafR;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> barkA, barkN;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;

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

	LoadTexture(AssetPath + L"Textures/leaves_albedo.png", leafA);
	LoadTexture(AssetPath + L"Textures/leaves_normals.png", leafN);
	LoadTexture(AssetPath + L"Textures/leaves_rough.png", leafR);

	LoadTexture(AssetPath + L"Textures/bark_albedo.jpg", barkA);
	LoadTexture(AssetPath + L"Textures/bark_normals.png", barkN);

	LoadTexture(AssetPath + L"Textures/noise_1.png", noiseTexture);
#undef LoadTexture


	// Load shaders (some are saved for later)
	vertexShader = Graphics::LoadVertexShader(FixPath(L"VertexShader.cso").c_str());
	pixelShader = Graphics::LoadPixelShader(FixPath(L"PixelShaderPBR.cso").c_str());
	solidColorPS = Graphics::LoadPixelShader(FixPath(L"SolidColorPS.cso").c_str());
	Microsoft::WRL::ComPtr<ID3D11VertexShader> skyVS = Graphics::LoadVertexShader(FixPath(L"SkyVS.cso").c_str());
	Microsoft::WRL::ComPtr<ID3D11PixelShader> skyPS = Graphics::LoadPixelShader(FixPath(L"SkyPS.cso").c_str());

	// Load 3D models	
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>("Cube", FixPath(AssetPath + L"Meshes/cube.obj").c_str());
	std::shared_ptr<Mesh> cylinderMesh = std::make_shared<Mesh>("Cylinder", FixPath(AssetPath + L"Meshes/cylinder.obj").c_str());
	std::shared_ptr<Mesh> helixMesh = std::make_shared<Mesh>("Helix", FixPath(AssetPath + L"Meshes/helix.obj").c_str());
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>("Sphere", FixPath(AssetPath + L"Meshes/sphere.obj").c_str());
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>("Torus", FixPath(AssetPath + L"Meshes/torus.obj").c_str());
	std::shared_ptr<Mesh> quadMesh = std::make_shared<Mesh>("Quad", FixPath(AssetPath + L"Meshes/quad.obj").c_str());
	std::shared_ptr<Mesh> quad2sidedMesh = std::make_shared<Mesh>("Double-Sided Quad", FixPath(AssetPath + L"Meshes/quad_double_sided.obj").c_str());

	std::shared_ptr<Mesh> trunkMesh = std::make_shared<Mesh>("Tree Trunk", FixPath(AssetPath + L"Meshes/tree_trunk.obj").c_str());
	std::shared_ptr<Mesh> leafMesh = std::make_shared<Mesh>("Tree Leaves", FixPath(AssetPath + L"Meshes/tree_leaves.obj").c_str());

	// Add all meshes to vector
	meshes.insert(meshes.end(), { cubeMesh, cylinderMesh, helixMesh, sphereMesh, torusMesh, quadMesh, quad2sidedMesh, trunkMesh, leafMesh });
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
	cobbleMat2x->AddSampler(0, sampler);
	cobbleMat2x->AddTextureSRV(0, cobbleA);
	cobbleMat2x->AddTextureSRV(1, cobbleN);
	cobbleMat2x->AddTextureSRV(2, cobbleR);
	cobbleMat2x->AddTextureSRV(3, cobbleM);

	std::shared_ptr<Material> cobbleMat4x = std::make_shared<Material>("Cobblestone (4x Scale)", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4x->AddSampler(0, sampler);
	cobbleMat4x->AddTextureSRV(0, cobbleA);
	cobbleMat4x->AddTextureSRV(1, cobbleN);
	cobbleMat4x->AddTextureSRV(2, cobbleR);
	cobbleMat4x->AddTextureSRV(3, cobbleM);

	std::shared_ptr<Material> floorMat = std::make_shared<Material>("Metal Floor", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	floorMat->AddSampler(0, sampler);
	floorMat->AddTextureSRV(0, floorA);
	floorMat->AddTextureSRV(1, floorN);
	floorMat->AddTextureSRV(2, floorR);
	floorMat->AddTextureSRV(3, floorM);

	std::shared_ptr<Material> paintMat = std::make_shared<Material>("Blue Paint", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	paintMat->AddSampler(0, sampler);
	paintMat->AddTextureSRV(0, paintA);
	paintMat->AddTextureSRV(1, paintN);
	paintMat->AddTextureSRV(2, paintR);
	paintMat->AddTextureSRV(3, paintM);

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>("Scratched Paint", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	scratchedMat->AddSampler(0, sampler);
	scratchedMat->AddTextureSRV(0, scratchedA);
	scratchedMat->AddTextureSRV(1, scratchedN);
	scratchedMat->AddTextureSRV(2, scratchedR);
	scratchedMat->AddTextureSRV(3, scratchedM);

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>("Bronze", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	bronzeMat->AddSampler(0, sampler);
	bronzeMat->AddTextureSRV(0, bronzeA);
	bronzeMat->AddTextureSRV(1, bronzeN);
	bronzeMat->AddTextureSRV(2, bronzeR);
	bronzeMat->AddTextureSRV(3, bronzeM);

	std::shared_ptr<Material> roughMat = std::make_shared<Material>("Rough Metal", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	roughMat->AddSampler(0, sampler);
	roughMat->AddTextureSRV(0, roughA);
	roughMat->AddTextureSRV(1, roughN);
	roughMat->AddTextureSRV(2, roughR);
	roughMat->AddTextureSRV(3, roughM);

	std::shared_ptr<Material> woodMat = std::make_shared<Material>("Wood", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	woodMat->AddSampler(0, sampler);
	woodMat->AddTextureSRV(0, woodA);
	woodMat->AddTextureSRV(1, woodN);
	woodMat->AddTextureSRV(2, woodR);
	woodMat->AddTextureSRV(3, woodM);

	// Create transparent materials
	std::shared_ptr<Material> fenceMat = std::make_shared<Material>("Fence", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 1), XMFLOAT2(0, 0), true);
	fenceMat->AddSampler(0, sampler);
	fenceMat->AddTextureSRV(0, fenceA);
	fenceMat->AddTextureSRV(1, fenceN);
	fenceMat->AddTextureSRV(2, fenceR);
	fenceMat->AddTextureSRV(3, fenceM);

	std::shared_ptr<Material> latticeMat = std::make_shared<Material>("Lattice", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 0.5f), XMFLOAT2(0, 0), true);
	latticeMat->AddSampler(0, sampler);
	latticeMat->AddTextureSRV(0, latticeA);
	latticeMat->AddTextureSRV(1, latticeN);
	latticeMat->AddTextureSRV(2, latticeR);
	latticeMat->AddTextureSRV(3, latticeM);

	std::shared_ptr<Material> glassWindowMat = std::make_shared<Material>("Glass Window", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 0.5f), XMFLOAT2(0, 0), true);
	glassWindowMat->AddSampler(0, sampler);
	glassWindowMat->AddTextureSRV(0, glassWindowA);
	glassWindowMat->AddTextureSRV(1, glassWindowN);
	glassWindowMat->AddTextureSRV(2, glassWindowR);
	glassWindowMat->AddTextureSRV(3, glassWindowM);

	std::shared_ptr<Material> glassPatternMat = std::make_shared<Material>("Glass Pattern", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 1), XMFLOAT2(0, 0), true);
	glassPatternMat->AddSampler(0, sampler);
	glassPatternMat->AddTextureSRV(0, glassPatternA);
	glassPatternMat->AddTextureSRV(1, glassPatternN);
	glassPatternMat->AddTextureSRV(2, glassPatternR);
	glassPatternMat->AddTextureSRV(3, glassPatternM);

	// Alpha clip
	std::shared_ptr<Material> clipMat = std::make_shared<Material>("Alpha Clip", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2), XMFLOAT2(0, 0), false, 0.5f);
	clipMat->AddSampler(0, sampler);
	clipMat->AddTextureSRV(0, paintA);
	clipMat->AddTextureSRV(1, bronzeN);
	clipMat->AddTextureSRV(2, bronzeR);
	clipMat->AddTextureSRV(3, bronzeM);
	clipMat->AddTextureSRV(4, noiseTexture);

	// Tree
	std::shared_ptr<Material> barkMat = std::make_shared<Material>("Bark", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 1), XMFLOAT2(0, 0), false);
	barkMat->AddSampler(0, sampler);
	barkMat->AddTextureSRV(0, barkA);
	barkMat->AddTextureSRV(1, roughN); // Using general rough normal map
	barkMat->AddTextureSRV(2, bronzeM); // 100% rough (white)
	barkMat->AddTextureSRV(3, paintM); // Non-metal (black)

	std::shared_ptr<Material> leafMat = std::make_shared<Material>("Leaf", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 1), XMFLOAT2(0, 0), false, 0.4f);
	leafMat->AddSampler(0, sampler);
	leafMat->AddTextureSRV(0, leafA);
	leafMat->AddTextureSRV(1, leafN);
	leafMat->AddTextureSRV(2, bronzeM); // 100% rough (white)
	leafMat->AddTextureSRV(3, paintM); // Non-metal (black)

	// Add materials to list
	materials.insert(materials.end(), { 
		cobbleMat2x, cobbleMat4x,floorMat, paintMat, scratchedMat, bronzeMat, roughMat, woodMat, 
		fenceMat, latticeMat, glassWindowMat, glassPatternMat,
		leafMat, barkMat, clipMat });

	// === Create the "randomized" entities, with a static floor ===========
	for (int i = 0; i < randomEntityCount; i++)
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

	std::shared_ptr<GameEntity> floor = std::make_shared<GameEntity>(cubeMesh, cobbleMat4x);
	floor->GetTransform()->SetScale(25, 25, 25);
	floor->GetTransform()->SetPosition(0, -27, 0);
	entities.push_back(floor);

	std::shared_ptr<GameEntity> treeTrunk = std::make_shared<GameEntity>(trunkMesh, barkMat);
	treeTrunk->GetTransform()->SetScale(3.0f);
	treeTrunk->GetTransform()->SetPosition(0, -2, 20);
	entities.push_back(treeTrunk);

	std::shared_ptr<GameEntity> treeLeaves = std::make_shared<GameEntity>(leafMesh, leafMat);
	treeLeaves->GetTransform()->SetScale(treeTrunk->GetTransform()->GetScale());
	treeLeaves->GetTransform()->SetPosition(treeTrunk->GetTransform()->GetPosition());
	entities.push_back(treeLeaves);

	// Clip entity
	clipEntity = std::make_shared<GameEntity>(sphereMesh, clipMat);
	clipEntity->GetTransform()->SetScale(5.0f);
	clipEntity->GetTransform()->SetPosition(10, 5, 20);
	entities.push_back(clipEntity);

	// Dither entity
	ditherEntity = std::make_shared<GameEntity>(sphereMesh, clipMat);
	ditherEntity->GetTransform()->SetScale(5.0f);
	ditherEntity->GetTransform()->SetPosition(-10, 5, 20);
	entities.push_back(ditherEntity);

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

	Light dir4 = {};
	dir4.Type = LIGHT_TYPE_DIRECTIONAL;
	dir4.Direction = XMFLOAT3(0, 1, 0);
	dir4.Color = XMFLOAT3(0.2f, 0.2f, 0.2f);
	dir4.Intensity = 1.0f;

	// Add light to the list
	lights.push_back(dir1);
	lights.push_back(dir2);
	lights.push_back(dir3);
	lights.push_back(dir4);

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
	// (up to the random entity count)
	for (int i = 0; i < entities.size() && i < randomEntityCount; i++)
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

	// Update special entities
	clipEntity->GetMaterial()->SetAlphaClipThreshold(sin(totalTime) * 0.3f + 0.5f); // ~0.2f - 0.8f

	XMFLOAT3 ditherPos = ditherEntity->GetTransform()->GetPosition();
	ditherPos.z = sin(totalTime * 0.25f) * 50.0f;
	ditherEntity->GetTransform()->SetPosition(ditherPos);

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

		// If it's alpha clip, assume we want the back side too
		float clip = e->GetMaterial()->GetAlphaClipThreshold();
		if (clip >= 0.0f)
		{
			Graphics::Context->RSSetState(backfaceRasterState.Get());
			DrawOneEntity(e, totalTime, true);
			Graphics::Context->RSSetState(0);
		}
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
	// Grab the material and it have bind its resources (textures and samplers) and shaders
	std::shared_ptr<Material> mat = entity->GetMaterial();
	mat->BindTexturesAndSamplers();
	Graphics::Context->VSSetShader(mat->GetVertexShader().Get(), 0, 0);
	Graphics::Context->PSSetShader(mat->GetPixelShader().Get(), 0, 0);

	// Set vertex shader data
	VertexShaderExternalData vsData{};
	vsData.worldMatrix = entity->GetTransform()->GetWorldMatrix();
	vsData.worldInvTransMatrix = entity->GetTransform()->GetWorldInverseTransposeMatrix();
	vsData.viewMatrix = camera->GetView();
	vsData.projectionMatrix = camera->GetProjection();
	Graphics::FillAndBindNextConstantBuffer(&vsData, sizeof(VertexShaderExternalData), D3D11_VERTEX_SHADER, 0);

	// Set pixel shader data (mostly coming from the material)
	PixelShaderExternalData psData{};
	memcpy(&psData.lights, &lights[0], sizeof(Light) * lights.size());
	psData.lightCount = lightOptions.LightCount;
	psData.ambientColor = XMFLOAT3(0,0,0);
	psData.cameraPosition = camera->GetTransform()->GetPosition();
	psData.colorTint = mat->GetColorTint();
	psData.uvOffset = mat->GetUVOffset();
	psData.uvScale = mat->GetUVScale();
	float clip = entity->GetMaterial()->GetAlphaClipThreshold();
	psData.alphaClipThreshold = clip >= 0.0f && transparencyOptions.AlphaClippingOn ? clip : -1.0f;
	psData.flipNormal = (int)flipNormal;
	psData.useNoiseForAlphaClip = (int)(entity == clipEntity);
	// Only dither for one entity
	psData.fadeDistStart = entity == ditherEntity ? 20.0f : -1.0f;
	psData.fadeDistEnd = entity == ditherEntity ? 50.0f : -1.0f;
	Graphics::FillAndBindNextConstantBuffer(&psData, sizeof(PixelShaderExternalData), D3D11_PIXEL_SHADER, 0);

	// Draw one entity
	entity->Draw();
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
	Graphics::Context->VSSetShader(vertexShader.Get(), 0, 0);
	Graphics::Context->PSSetShader(solidColorPS.Get(), 0, 0);

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

		// Set vertex shader data
		VertexShaderExternalData vsData{};
		vsData.worldMatrix = world;
		vsData.viewMatrix = camera->GetView();
		vsData.projectionMatrix = camera->GetProjection();
		Graphics::FillAndBindNextConstantBuffer(&vsData, sizeof(VertexShaderExternalData), D3D11_VERTEX_SHADER, 0);

		// Set up the pixel shader data
		XMFLOAT3 finalColor = light.Color;
		finalColor.x *= light.Intensity;
		finalColor.y *= light.Intensity;
		finalColor.z *= light.Intensity;
		Graphics::FillAndBindNextConstantBuffer(&finalColor, sizeof(XMFLOAT3), D3D11_PIXEL_SHADER, 0);

		// Draw
		Graphics::Context->DrawIndexed(indexCount, 0, 0);
	}

}