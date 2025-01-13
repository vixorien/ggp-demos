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

	// Set up entities
	LoadAssetsAndCreateEntities();

	// Set initial graphics API state
	//  - These settings persist until we change them
	//  - Some of these, like the primitive topology & input layout, probably won't change
	//  - Others, like setting shaders, will need to be moved elsewhere later
	{
		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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

	// Start with gamma correction ON
	gammaCorrection = true;
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
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
		rockSRV,
		rockNormalsSRV,
		cushionSRV,
		cushionNormalsSRV,
		cobblestoneSRV,
		cobblestoneNormalsSRV,
		cobblestoneSpecularSRV;

	// Quick pre-processor macro for simplifying texture loading calls below
#define LoadTexture(path, srv) CreateWICTextureFromFile(Graphics::Device.Get(), Graphics::Context.Get(), FixPath(path).c_str(), 0, srv.GetAddressOf());
	LoadTexture(AssetPath + L"Textures/rock.png", rockSRV);
	LoadTexture(AssetPath + L"Textures/rock_normals.png", rockNormalsSRV);
	LoadTexture(AssetPath + L"Textures/cushion.png", cushionSRV);
	LoadTexture(AssetPath + L"Textures/cushion_normals.png", cushionNormalsSRV);
	LoadTexture(AssetPath + L"Textures/cobblestone.png", cobblestoneSRV);
	LoadTexture(AssetPath + L"Textures/cobblestone_normals.png", cobblestoneNormalsSRV);
	LoadTexture(AssetPath + L"Textures/cobblestone_specular.png", cobblestoneSpecularSRV);
#undef LoadTexture


	// Load shaders
	std::shared_ptr<SimpleVertexShader> basicVertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"VertexShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> basicPixelShader = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShader.cso").c_str());
	std::shared_ptr<SimplePixelShader> normalMapPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"NormalMapPS.cso").c_str());
	std::shared_ptr<SimplePixelShader> lightAndEnvMapPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"LightingAndEnvMapPS.cso").c_str());
	std::shared_ptr<SimplePixelShader> envMapOnlyPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"EnvMapOnlyPS.cso").c_str());
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



	// Create basic materials (no normal maps)
	std::shared_ptr<Material> matRock = std::make_shared<Material>("Rock", basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRock->AddSampler("BasicSampler", sampler);
	matRock->AddTextureSRV("SurfaceTexture", rockSRV);

	std::shared_ptr<Material> matCushion = std::make_shared<Material>("Cushion", basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushion->AddSampler("BasicSampler", sampler);
	matCushion->AddTextureSRV("SurfaceTexture", cushionSRV);

	std::shared_ptr<Material> matCobblestone = std::make_shared<Material>("Cobblestone", basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestone->AddSampler("BasicSampler", sampler);
	matCobblestone->AddTextureSRV("SurfaceTexture", cobblestoneSRV);
	matCobblestone->AddTextureSRV("SpecularMap", cobblestoneSpecularSRV);


	// Create normal mapped materials
	std::shared_ptr<Material> matRockNormalMap = std::make_shared<Material>("Rock with Normal Map", normalMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRockNormalMap->AddSampler("BasicSampler", sampler);
	matRockNormalMap->AddTextureSRV("SurfaceTexture", rockSRV);
	matRockNormalMap->AddTextureSRV("NormalMap", rockNormalsSRV);

	std::shared_ptr<Material> matCushionNormalMap = std::make_shared<Material>("Cushion with Normal Map", normalMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushionNormalMap->AddSampler("BasicSampler", sampler);
	matCushionNormalMap->AddTextureSRV("SurfaceTexture", cushionSRV);
	matCushionNormalMap->AddTextureSRV("NormalMap", cushionNormalsSRV);

	std::shared_ptr<Material> matCobblestoneNormalMap = std::make_shared<Material>("Cobblestone with Normal Map", normalMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestoneNormalMap->AddSampler("BasicSampler", sampler);
	matCobblestoneNormalMap->AddTextureSRV("SurfaceTexture", cobblestoneSRV);
	matCobblestoneNormalMap->AddTextureSRV("NormalMap", cobblestoneNormalsSRV);
	matCobblestoneNormalMap->AddTextureSRV("SpecularMap", cobblestoneSpecularSRV);


	// Create normal mapped & environment mapped materials ---------------------
	std::shared_ptr<Material> matRockLitEnvMap = std::make_shared<Material>("Rock with Env Mapping", lightAndEnvMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRockLitEnvMap->AddSampler("BasicSampler", sampler);
	matRockLitEnvMap->AddTextureSRV("SurfaceTexture", rockSRV);
	matRockLitEnvMap->AddTextureSRV("NormalMap", rockNormalsSRV);
	matRockLitEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	std::shared_ptr<Material> matCushionLitEnvMap = std::make_shared<Material>("Cushion with Env Mapping", lightAndEnvMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushionLitEnvMap->AddSampler("BasicSampler", sampler);
	matCushionLitEnvMap->AddTextureSRV("SurfaceTexture", cushionSRV);
	matCushionLitEnvMap->AddTextureSRV("NormalMap", cushionNormalsSRV);
	matCushionLitEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	std::shared_ptr<Material> matCobblestoneLitEnvMap = std::make_shared<Material>("Cobblestone with Env Mapping", lightAndEnvMapPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestoneLitEnvMap->AddSampler("BasicSampler", sampler);
	matCobblestoneLitEnvMap->AddTextureSRV("SurfaceTexture", cobblestoneSRV);
	matCobblestoneLitEnvMap->AddTextureSRV("NormalMap", cobblestoneNormalsSRV);
	matCobblestoneLitEnvMap->AddTextureSRV("SpecularMap", cobblestoneSpecularSRV);
	matCobblestoneLitEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	// Create environment mapped only materials ---------------------
	std::shared_ptr<Material> matRockEnvMap = std::make_shared<Material>("Rock Env Map Only", envMapOnlyPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false);
	matRockEnvMap->AddSampler("BasicSampler", sampler);
	matRockEnvMap->AddTextureSRV("NormalMap", rockNormalsSRV);
	matRockEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	std::shared_ptr<Material> matCushionEnvMap = std::make_shared<Material>("Cushion Env Map Only", envMapOnlyPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, false, XMFLOAT2(2, 2));
	matCushionEnvMap->AddSampler("BasicSampler", sampler);
	matCushionEnvMap->AddTextureSRV("NormalMap", cushionNormalsSRV);
	matCushionEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	std::shared_ptr<Material> matCobblestoneEnvMap = std::make_shared<Material>("Cobblestone Env Map Only", envMapOnlyPS, basicVertexShader, XMFLOAT3(1, 1, 1), 0.0f, true);
	matCobblestoneEnvMap->AddSampler("BasicSampler", sampler);
	matCobblestoneEnvMap->AddTextureSRV("NormalMap", cobblestoneNormalsSRV);
	matCobblestoneEnvMap->AddTextureSRV("EnvironmentMap", sky->GetSkyTexture());

	// Add all materials to vector
	materials.insert(materials.end(), { 
		matRock, matCushion, matCobblestone, 
		matRockNormalMap, matCushionNormalMap, matCobblestoneNormalMap,
		matRockLitEnvMap, matCushionLitEnvMap, matCobblestoneLitEnvMap,
		matRockEnvMap, matCushionEnvMap, matCobblestoneEnvMap });

	// Create many sets of entities - with and without normal maps and env map
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matRock));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matRock));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCushion));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCushion));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCobblestone));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCobblestone));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matRockNormalMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matRockNormalMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCushionNormalMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCushionNormalMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCobblestoneNormalMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCobblestoneNormalMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matRockLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matRockLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCushionLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCushionLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCobblestoneLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCobblestoneLitEnvMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matRockEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matRockEnvMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCushionEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCushionEnvMap));
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matCobblestoneEnvMap));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matCobblestoneEnvMap));

	// Line up the entities like so:
	//
	//  c  s  c  s  c  s  <-- Regular
	//
	//  c  s  c  s  c  s  <-- Normal mapped
	//
	//  c  s  c  s  c  s  <-- Lit & Environment mapped
	//
	//  c  s  c  s  c  s  <-- Environment mapped only
	//
	int i = 0;
	for (float y = 4.5; y >= -4.5; y -= 3)
	{
		for (float x = -7.5f; x <= 7.5f; x += 3)
		{
			entities[i++]->GetTransform()->MoveAbsolute(x, y, 0);
		}
	}

	// Create lights - Must respect the
	// max lights defined in the pixel shader!
	// Note: directions are currently being normalized in the shader
	Light dirLight1 = {};
	dirLight1.Color = XMFLOAT3(1, 0, 0);
	dirLight1.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight1.Intensity = 1.0f;
	dirLight1.Direction = XMFLOAT3(1, 0, 0);

	Light dirLight2 = {};
	dirLight2.Color = XMFLOAT3(1, 1, 1);
	dirLight2.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight2.Intensity = 1.0f;
	dirLight2.Direction = XMFLOAT3(-0.25f, -1, 0.75f);

	Light dirLight3 = {};
	dirLight3.Color = XMFLOAT3(0, 0, 1);
	dirLight3.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight3.Intensity = 1.0f;
	dirLight3.Direction = XMFLOAT3(-1, 1, -0.5f);

	// Point lights set to zero intensity for this demo
	// Alternatively, they could be removed entirely
	Light pointLight1 = {};
	pointLight1.Color = XMFLOAT3(1, 1, 1);
	pointLight1.Type = LIGHT_TYPE_POINT;
	pointLight1.Intensity = 0.0f;
	pointLight1.Position = XMFLOAT3(-1.5f, 0, 0);
	pointLight1.Range = 10.0f;

	Light pointLight2 = {};
	pointLight2.Color = XMFLOAT3(1, 1, 1);
	pointLight2.Type = LIGHT_TYPE_POINT;
	pointLight2.Intensity = 0.0f;
	pointLight2.Position = XMFLOAT3(1.5f, 0, 0);
	pointLight2.Range = 10.0f;

	// Add all lights to the list
	lights.push_back(dirLight1);
	lights.push_back(dirLight2);
	lights.push_back(dirLight3);
	lights.push_back(pointLight1);
	lights.push_back(pointLight2);

	// Normalize directions of all non-point lights
	for (int i = 0; i < lights.size(); i++)
		if (lights[i].Type != LIGHT_TYPE_POINT)
			XMStoreFloat3(
				&lights[i].Direction, 
				XMVector3Normalize(XMLoadFloat3(&lights[i].Direction))
			);
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
	BuildUI(camera, meshes, entities, materials, lights, ambientColor, gammaCorrection);

	// Example input checking: Quit if the escape key is pressed
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();

	// Spin the 3D models
	for (auto& e : entities)
	{
		e->GetTransform()->Rotate(0, deltaTime * 0.25f, 0);
	}

	// Update the camera this frame
	camera->Update(deltaTime);
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

	// DRAW geometry
	// Loop through the game entities and draw each one
	// - Note: A constant buffer has already been bound to
	//   the vertex shader stage of the pipeline (see Init above)
	for (auto& e : entities)
	{
		// Set total time on this entity's material's pixel shader
		// Note: If the shader doesn't have this variable, nothing happens
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetFloat("time", totalTime);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("gammaCorrection", gammaCorrection);

		// Draw one entity
		e->Draw(camera);
	}

	// Draw the sky after all regular entities
	sky->Draw(camera);

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
