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
	GenerateLights();

	// Set up defaults for lighting options
	lightOptions = {
		.LightCount = 3,
		.GammaCorrection = true,
		.UseAlbedoTexture = true,
		.UseMetalMap = true,
		.UseNormalMap = true,
		.UseRoughnessMap = true,
		.UsePBR = true,
		.FreezeLightMovement = true,
		.DrawLights = true,
		.ShowSkybox = true,
		.UseBurleyDiffuse = false,
		.AmbientColor = XMFLOAT3(0,0,0)
	};

	parallaxOptions = {
		.SampleCount = 64,
		.HeightScale = 0.1f
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
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shapesA, shapesN, shapesR, shapesM, shapesH;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> stonesA, stonesN, stonesR, stonesM, stonesH;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> leatherA, leatherN, leatherR, leatherM, leatherH;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bricksA, bricksN, bricksR, bricksM, bricksH;

	// Quick pre-processor macro for simplifying texture loading calls below
#define LoadTexture(path, srv) CreateWICTextureFromFile(Graphics::Device.Get(), Graphics::Context.Get(), FixPath(path).c_str(), 0, srv.GetAddressOf());
	LoadTexture(AssetPath + L"Textures/PBR/wood_albedo.png", shapesA);
	LoadTexture(AssetPath + L"Textures/shapes_normals.png", shapesN);
	LoadTexture(AssetPath + L"Textures/PBR/wood_roughness.png", shapesR);
	LoadTexture(AssetPath + L"Textures/PBR/wood_metal.png", shapesM);
	LoadTexture(AssetPath + L"Textures/shapes_height.png", shapesH);

	LoadTexture(AssetPath + L"Textures/stones.png", stonesA);
	LoadTexture(AssetPath + L"Textures/stones_normals.png", stonesN);
	LoadTexture(AssetPath + L"Textures/stones_height.png", stonesR);
	LoadTexture(AssetPath + L"Textures/PBR/wood_metal.png", stonesM); // White
	LoadTexture(AssetPath + L"Textures/stones_height.png", stonesH);

	LoadTexture(AssetPath + L"Textures/PBR/leather_albedo.jpg", leatherA);
	LoadTexture(AssetPath + L"Textures/PBR/leather_normals.jpg", leatherN);
	LoadTexture(AssetPath + L"Textures/PBR/leather_roughness.jpg", leatherR);
	LoadTexture(AssetPath + L"Textures/PBR/leather_metal.jpg", leatherM);
	LoadTexture(AssetPath + L"Textures/PBR/leather_height.jpg", leatherH);

	LoadTexture(AssetPath + L"Textures/PBR/bricks_albedo.jpg", bricksA);
	LoadTexture(AssetPath + L"Textures/PBR/bricks_normals.jpg", bricksN);
	LoadTexture(AssetPath + L"Textures/PBR/bricks_roughness.jpg", bricksR);
	LoadTexture(AssetPath + L"Textures/PBR/bricks_metal.jpg", bricksM);
	LoadTexture(AssetPath + L"Textures/PBR/bricks_height.jpg", bricksH);
#undef LoadTexture


	// Load shaders (some are saved for later)
	vertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"VertexShader.cso").c_str());
	pixelShader = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShader.cso").c_str());
	pixelShaderPBR = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"PixelShaderPBR.cso").c_str());
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
	std::shared_ptr<Material> parallaxShapesMat = std::make_shared<Material>("Shapes", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	parallaxShapesMat->AddSampler("BasicSampler", sampler);
	parallaxShapesMat->AddTextureSRV("Albedo", shapesA);
	parallaxShapesMat->AddTextureSRV("NormalMap", shapesN);
	parallaxShapesMat->AddTextureSRV("RoughnessMap", shapesR);
	parallaxShapesMat->AddTextureSRV("MetalMap", shapesM);
	parallaxShapesMat->AddTextureSRV("HeightMap", shapesH);

	std::shared_ptr<Material> parallaxStonesMat = std::make_shared<Material>("Stones", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	parallaxStonesMat->AddSampler("BasicSampler", sampler);
	parallaxStonesMat->AddTextureSRV("Albedo", stonesA);
	parallaxStonesMat->AddTextureSRV("NormalMap", stonesN);
	parallaxStonesMat->AddTextureSRV("RoughnessMap", stonesR);
	parallaxStonesMat->AddTextureSRV("MetalMap", stonesM);
	parallaxStonesMat->AddTextureSRV("HeightMap", stonesH);

	std::shared_ptr<Material> parallaxLeatherMat = std::make_shared<Material>("Leather", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	parallaxLeatherMat->AddSampler("BasicSampler", sampler);
	parallaxLeatherMat->AddTextureSRV("Albedo", leatherA);
	parallaxLeatherMat->AddTextureSRV("NormalMap", leatherN);
	parallaxLeatherMat->AddTextureSRV("RoughnessMap", leatherR);
	parallaxLeatherMat->AddTextureSRV("MetalMap", leatherM);
	parallaxLeatherMat->AddTextureSRV("HeightMap", leatherH);

	std::shared_ptr<Material> parallaxBricksMat = std::make_shared<Material>("Bricks", pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	parallaxBricksMat->AddSampler("BasicSampler", sampler);
	parallaxBricksMat->AddTextureSRV("Albedo", bricksA);
	parallaxBricksMat->AddTextureSRV("NormalMap", bricksN);
	parallaxBricksMat->AddTextureSRV("RoughnessMap", bricksR);
	parallaxBricksMat->AddTextureSRV("MetalMap", bricksM);
	parallaxBricksMat->AddTextureSRV("HeightMap", bricksH);


	// Add materials to list
	materials.insert(materials.end(), { parallaxShapesMat, parallaxStonesMat, parallaxLeatherMat, parallaxBricksMat });

	// === Create the scene ===
	std::shared_ptr<GameEntity> shapesCube = std::make_shared<GameEntity>(cubeMesh, parallaxShapesMat);
	//shapesCube->GetTransform()->SetScale(3);
	shapesCube->GetTransform()->SetPosition(0, 0, 0);
	entities.push_back(shapesCube);

	std::shared_ptr<GameEntity> leatherCube = std::make_shared<GameEntity>(cubeMesh, parallaxLeatherMat);
	//leatherCube->GetTransform()->SetScale(3);
	leatherCube->GetTransform()->SetPosition(-5, 0, 0);
	entities.push_back(leatherCube);

	std::shared_ptr<GameEntity> bricksCube = std::make_shared<GameEntity>(cubeMesh, parallaxBricksMat);
	//bricksCube->GetTransform()->SetScale(3);
	bricksCube->GetTransform()->SetPosition(5, 0, 0);
	entities.push_back(bricksCube);

	std::shared_ptr<GameEntity> plane = std::make_shared<GameEntity>(quad2sidedMesh, parallaxStonesMat);
	plane->GetTransform()->SetScale(2);
	plane->GetTransform()->SetPosition(0, -5, 0);
	plane->GetTransform()->SetRotation(-XM_PIDIV2, 0, 0);
	entities.push_back(plane);
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
	BuildUI(camera, meshes, entities, materials, lights, lightOptions, parallaxOptions);

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

	// DRAW geometry
	// Loop through the game entities and draw each one
	// - Note: A constant buffer has already been bound to
	//   the vertex shader stage of the pipeline (see Init above)
	for (auto& e : entities)
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

		ps->SetInt("parallaxSamples", parallaxOptions.SampleCount);
		ps->SetFloat("heightScale", parallaxOptions.HeightScale);


		// Draw one entity
		e->Draw(camera);
	}

	// Draw the sky after all regular entities
	if (lightOptions.ShowSkybox) sky->Draw(camera);

	// Draw the light sources
	if (lightOptions.DrawLights) DrawLightSources();

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
