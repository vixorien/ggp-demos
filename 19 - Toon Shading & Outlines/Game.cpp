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

	// Set up the scene and create lights
	LoadAssetsAndCreateEntities();
	GenerateLights();

	// Set initial graphics API state
	Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the camera
	camera = std::make_shared<FPSCamera>(
		XMFLOAT3(0.0f, 0.0f, -25.0f),	// Position
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
	// Set up the initial post process resources
	ResizePostProcessResources();

	// Sampler states
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	
	// Basic sampler (aniso wrap)
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; // What happens outside the 0-1 uv range?
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;		// How do we handle sampling "between" pixels?
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	Graphics::Device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());

	// Create a clamp sampler too
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	Graphics::Device->CreateSamplerState(&sampDesc, clampSampler.GetAddressOf());

	// Outline rasterizer mode for inside out mesh technique
	D3D11_RASTERIZER_DESC outlineRS = {};
	outlineRS.CullMode = D3D11_CULL_FRONT;
	outlineRS.FillMode = D3D11_FILL_SOLID;
	outlineRS.DepthClipEnable = true;
	Graphics::Device->CreateRasterizerState(&outlineRS, insideOutRasterState.GetAddressOf());


	// Textures
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteSRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(1, 1, 1, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> greySRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(0.5f, 0.5f, 0.5f, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> blackSRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(0, 0, 0, 1));
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> flatNormalsSRV = CreateSolidColorTextureSRV(2, 2, XMFLOAT4(0.5f, 0.5f, 1.0f, 1));

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cushionA, cushionN;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> crateA;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mandoA;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> containerA;

	// Quick pre-processor macro for simplifying texture loading calls below
#define LoadTexture(path, srv) CreateWICTextureFromFile(Graphics::Device.Get(), Graphics::Context.Get(), FixPath(path).c_str(), 0, srv.GetAddressOf());
	LoadTexture(L"../../../Assets/Textures/cushion.png", cushionA);
	LoadTexture(L"../../../Assets/Textures/cushion_normals.png", cushionN);
	LoadTexture(L"../../../Assets/Textures/PBR/crate_wood_albedo.png", crateA);
	LoadTexture(L"../../../Assets/Textures/mando.png", mandoA);
	LoadTexture(L"../../../Assets/Textures/container.png", containerA);

	LoadTexture(L"../../../Assets/Textures/Ramps/toonRamp1.png", toonRamp1);
	LoadTexture(L"../../../Assets/Textures/Ramps/toonRamp2.png", toonRamp2);
	LoadTexture(L"../../../Assets/Textures/Ramps/toonRamp3.png", toonRamp3);
	LoadTexture(L"../../../Assets/Textures/Ramps/toonRampSpecular.png", specularRamp);
#undef LoadTexture

	// Load shaders (some are saved for later)
	vertexShader = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"VertexShader.cso").c_str());
	insideOutVS = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"InsideOutVS.cso").c_str());
	fullscreenVS = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"FullscreenTriangleVS.cso").c_str());
	simpleTexturePS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SimpleTexturePS.cso").c_str());
	solidColorPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SolidColorPS.cso").c_str());
	sobelFilterPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SobelFilterPS.cso").c_str());
	silhouettePS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SilhouettePS.cso").c_str());
	depthNormalOutlinePS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"DepthNormalOutlinePS.cso").c_str());

	std::shared_ptr<SimplePixelShader> toonPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"ToonPS.cso").c_str());
	std::shared_ptr<SimpleVertexShader> skyVS = std::make_shared<SimpleVertexShader>(Graphics::Device, Graphics::Context, FixPath(L"SkyVS.cso").c_str());
	std::shared_ptr<SimplePixelShader> skyPS = std::make_shared<SimplePixelShader>(Graphics::Device, Graphics::Context, FixPath(L"SkyPS.cso").c_str());

	// Load 3D models	
	quadMesh = std::make_shared<Mesh>("Quad", FixPath(L"../../../Assets/Meshes/quad.obj").c_str());
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>("Cube", FixPath(L"../../../Assets/Meshes/cube.obj").c_str());
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>("Sphere", FixPath(L"../../../Assets/Meshes/sphere.obj").c_str());
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>("Torus", FixPath(L"../../../Assets/Meshes/torus.obj").c_str());
	std::shared_ptr<Mesh> crateMesh = std::make_shared<Mesh>("Crate", FixPath(L"../../../Assets/Meshes/crate_wood.obj").c_str());
	std::shared_ptr<Mesh> mandoMesh = std::make_shared<Mesh>("Mando", FixPath(L"../../../Assets/Meshes/mando.obj").c_str());
	std::shared_ptr<Mesh> containerMesh = std::make_shared<Mesh>("Container", FixPath(L"../../../Assets/Meshes/container.obj").c_str());

	// Add all meshes to vector
	meshes.insert(meshes.end(), { quadMesh, cubeMesh, sphereMesh, torusMesh, crateMesh, mandoMesh, containerMesh });
	pointLightMesh = sphereMesh;

	// Create the sky
	sky = std::make_shared<Sky>(
		FixPath(L"../../../Assets/Skies/Clouds Blue/right.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/left.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/up.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/down.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/front.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/back.png").c_str(),
		cubeMesh,
		skyVS,
		skyPS,
		sampler);

	// Create basic materials
	std::shared_ptr<Material> whiteMat = std::make_shared<Material>("Toon White", toonPS, vertexShader, XMFLOAT3(1, 1, 1));
	whiteMat->AddSampler("BasicSampler", sampler);
	whiteMat->AddSampler("ClampSampler", clampSampler);
	whiteMat->AddTextureSRV("Albedo", whiteSRV);
	whiteMat->AddTextureSRV("NormalMap", flatNormalsSRV);
	whiteMat->AddTextureSRV("RoughnessMap", blackSRV);

	std::shared_ptr<Material> redMat = std::make_shared<Material>("Toon Red", toonPS, vertexShader, XMFLOAT3(0.8f, 0, 0));
	redMat->AddSampler("BasicSampler", sampler);
	redMat->AddSampler("ClampSampler", clampSampler);
	redMat->AddTextureSRV("Albedo", whiteSRV);
	redMat->AddTextureSRV("NormalMap", flatNormalsSRV);
	redMat->AddTextureSRV("RoughnessMap", blackSRV);

	std::shared_ptr<Material> detailedMat = std::make_shared<Material>("Toon Cushion", toonPS, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(2, 2));
	detailedMat->AddSampler("BasicSampler", sampler);
	detailedMat->AddSampler("ClampSampler", clampSampler);
	detailedMat->AddTextureSRV("Albedo", cushionA);
	detailedMat->AddTextureSRV("NormalMap", cushionN);
	detailedMat->AddTextureSRV("RoughnessMap", blackSRV);

	std::shared_ptr<Material> crateMat = std::make_shared<Material>("Toon Crate", toonPS, vertexShader, XMFLOAT3(1, 1, 1));
	crateMat->AddSampler("BasicSampler", sampler);
	crateMat->AddSampler("ClampSampler", clampSampler);
	crateMat->AddTextureSRV("Albedo", crateA);
	crateMat->AddTextureSRV("NormalMap", flatNormalsSRV);
	crateMat->AddTextureSRV("RoughnessMap", greySRV);

	std::shared_ptr<Material> mandoMat = std::make_shared<Material>("Toon Mando", toonPS, vertexShader, XMFLOAT3(1, 1, 1));
	mandoMat->AddSampler("BasicSampler", sampler);
	mandoMat->AddSampler("ClampSampler", clampSampler);
	mandoMat->AddTextureSRV("Albedo", mandoA);
	mandoMat->AddTextureSRV("NormalMap", flatNormalsSRV);
	mandoMat->AddTextureSRV("RoughnessMap", blackSRV);

	std::shared_ptr<Material> containerMat = std::make_shared<Material>("Toon Container", toonPS, vertexShader, XMFLOAT3(1, 1, 1));
	containerMat->AddSampler("BasicSampler", sampler);
	containerMat->AddSampler("ClampSampler", clampSampler);
	containerMat->AddTextureSRV("Albedo", containerA);
	containerMat->AddTextureSRV("NormalMap", flatNormalsSRV);
	containerMat->AddTextureSRV("RoughnessMap", greySRV);

	// Add materials to list
	materials.insert(materials.end(), { whiteMat, redMat, detailedMat, crateMat, mandoMat, containerMat });

	// === Create the entities =====================================
	std::shared_ptr<GameEntity> sphere = std::make_shared<GameEntity>(sphereMesh, whiteMat);
	sphere->GetTransform()->SetPosition(0, 0, 0);

	std::shared_ptr<GameEntity> torus = std::make_shared<GameEntity>(torusMesh, redMat);
	torus->GetTransform()->SetRotation(0, 0, XM_PIDIV2);
	torus->GetTransform()->SetPosition(0, -3, 0);

	std::shared_ptr<GameEntity> detailed = std::make_shared<GameEntity>(sphereMesh, detailedMat);
	//detailed->GetTransform()->SetScale(2.0f);
	detailed->GetTransform()->SetPosition(0, -6, 0);

	std::shared_ptr<GameEntity> mando = std::make_shared<GameEntity>(mandoMesh, mandoMat);
	mando->GetTransform()->SetPosition(0, -9, 0);

	std::shared_ptr<GameEntity> crate = std::make_shared<GameEntity>(crateMesh, crateMat);
	crate->GetTransform()->SetPosition(0, -12, 0);

	std::shared_ptr<GameEntity> container = std::make_shared<GameEntity>(containerMesh, containerMat);
	container->GetTransform()->SetPosition(0, -16, 0);
	container->GetTransform()->SetScale(0.075f);

	entities.push_back(sphere);
	entities.push_back(torus);
	entities.push_back(detailed);
	entities.push_back(mando);
	entities.push_back(crate);
	entities.push_back(container);
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

	// Resize post processes resources on window resize
	if(Graphics::Device)
		ResizePostProcessResources();
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
	BuildUI(camera, meshes, entities, materials, lights, options);

	// Example input checking: Quit if the escape key is pressed
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();

	// Update the camera this frame
	camera->Update(deltaTime);

	// Move lights
	for (int i = 0; i < options.LightCount && !options.FreezeLightMovement; i++)
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

	// Handle light count changes, clamped appropriately
	if (Input::KeyDown(VK_UP)) options.LightCount++;
	if (Input::KeyDown(VK_DOWN)) options.LightCount--;
	options.LightCount = max(1, min(MAX_LIGHTS, options.LightCount));
}


// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Any PRE-RENDER steps we need to take care of?
	// - Clearing the render target and depth buffer
	// - Usually post-processing related things, too
	PreRender();

	// Reset the silhouette ID before rendering any entities
	silhouetteID = 0;

	// Render entities with several different toon shading variations
	RenderEntitiesWithToonShading(ToonShadingNone, 0, true, XMFLOAT3(-6, 7.5f, 0));
	RenderEntitiesWithToonShading(ToonShadingConditionals, 0, true, XMFLOAT3(-3, 7.5f, 0));
	RenderEntitiesWithToonShading(ToonShadingRamp, toonRamp1, true, XMFLOAT3(0, 7.5f, 0));
	RenderEntitiesWithToonShading(ToonShadingRamp, toonRamp2, true, XMFLOAT3(3, 7.5f, 0));
	RenderEntitiesWithToonShading(ToonShadingRamp, toonRamp3, true, XMFLOAT3(6, 7.5f, 0));


	// Draw the sky after all regular entities
	sky->Draw(camera);

	// Draw the light sources
	if (options.DrawLights) DrawLightSources();

	// Draw sprites to show ramp textures
	if (options.ShowRampTextures)
	{
		DrawQuadAtLocation(toonRamp1, XMFLOAT3(0, 10, 0), XMFLOAT2(2, 2), XMFLOAT3(-XM_PIDIV2, 0, 0));
		DrawQuadAtLocation(toonRamp2, XMFLOAT3(3, 10, 0), XMFLOAT2(2, 2), XMFLOAT3(-XM_PIDIV2, 0, 0));
		DrawQuadAtLocation(toonRamp3, XMFLOAT3(6, 10, 0), XMFLOAT2(2, 2), XMFLOAT3(-XM_PIDIV2, 0, 0));
	}

	// Show specular ramp, too
	if(options.ShowSpecularRamp)
		DrawQuadAtLocation(specularRamp, XMFLOAT3(8.5f, 7.5f, 0), XMFLOAT2(2, 2), XMFLOAT3(-XM_PIDIV2, 0, 0));

	// Post-scene-render things now
	// - Usually post processing
	PostRender();

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

	for (int i = 0; i < options.LightCount; i++)
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

// --------------------------------------------------------
// Draws the given sprite (texture) at the specified location in 3D space
// --------------------------------------------------------
void Game::DrawQuadAtLocation(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv, DirectX::XMFLOAT3 position, DirectX::XMFLOAT2 scale, DirectX::XMFLOAT3 pitchYawRoll)
{
	// Turn on these shaders
	vertexShader->SetShader();
	simpleTexturePS->SetShader();

	// Set up vertex shader
	XMFLOAT4X4 world;
	XMStoreFloat4x4(&world, 
		XMMatrixScaling(0.5f * scale.x, -0.5f * scale.y, 1) *
		XMMatrixRotationRollPitchYaw(pitchYawRoll.x, pitchYawRoll.y, pitchYawRoll.z) *
		XMMatrixTranslation(position.x, position.y, position.z));
	vertexShader->SetMatrix4x4("world", world);
	vertexShader->SetMatrix4x4("view", camera->GetView());
	vertexShader->SetMatrix4x4("projection", camera->GetProjection());
	vertexShader->CopyAllBufferData();

	// Set up pixel shader
	simpleTexturePS->SetShaderResourceView("Pixels", srv);
	simpleTexturePS->SetSamplerState("BasicSampler", clampSampler);

	// Draw quad
	quadMesh->SetBuffersAndDraw();
}

// --------------------------------------------------------
// Renders entities, potentially with toon shading and 
// an offset to all of their positions
// --------------------------------------------------------
void Game::RenderEntitiesWithToonShading(ToonShadingType toonMode, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> toonRamp, bool offsetPositions, DirectX::XMFLOAT3 offset)
{
	// Loop through the game entities in the current scene and draw
	for (auto& e : entities)
	{
		// Set total time on this entity's material's pixel shader
		// Note: If the shader doesn't have this variable, nothing happens
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("lightCount", options.LightCount);
		ps->SetInt("toonShadingType", toonMode);

		// Need to set the silhouette ID if that's the outline mode
		if (options.OutlineMode == OutlineSilhouette)
		{
			ps->SetInt("silhouetteID", silhouetteID);
			silhouetteID++; // Increment, too!
		}

		// Set toon-shading textures if necessary
		if (toonMode == ToonShadingRamp)
		{
			ps->SetShaderResourceView("ToonRamp", toonRamp);
			ps->SetShaderResourceView("ToonRampSpecular", specularRamp);
		}

		// If we're overriding the position, save the old one
		XMFLOAT3 originalPos = e->GetTransform()->GetPosition();
		if (offsetPositions) e->GetTransform()->MoveAbsolute(offset);

		// Draw one entity
		e->Draw(camera);

		// Outline too?
		if (options.OutlineMode == OutlineInsideOut)
			DrawOutlineInsideOut(e, camera, 0.03f);

		// Replace the old position if necessary
		if (offsetPositions) e->GetTransform()->SetPosition(originalPos);
	}
}


// ------------------------------------------------------------
// Resizes (by releasing and re-creating) the resources
// required for post processing.  Note the useage of 
// ComPtr's .ReleaseAndGetAddressOf() method for this. 
// 
// We only need to do this at start-up and whenever the 
// window is resized.
// ------------------------------------------------------------
void Game::ResizePostProcessResources()
{
	// Reset all resources (releasing them)
	ppRTV.Reset();
	ppSRV.Reset();
	sceneNormalsRTV.Reset();
	sceneNormalsSRV.Reset();
	sceneDepthRTV.Reset();
	sceneDepthSRV.Reset();

	// Describe our textures
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = Window::Width();
	textureDesc.Height = Window::Height();
	textureDesc.ArraySize = 1;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // Will render to it and sample from it!
	textureDesc.CPUAccessFlags = 0;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.MipLevels = 1;
	textureDesc.MiscFlags = 0;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;

	// Create the color and normals textures
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ppTexture;
	Graphics::Device->CreateTexture2D(&textureDesc, 0, ppTexture.GetAddressOf());

	// Adjust the description for scene normals
	textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneNormalsTexture;
	Graphics::Device->CreateTexture2D(&textureDesc, 0, sceneNormalsTexture.GetAddressOf());

	// Adjust the description for the scene depths
	textureDesc.Format = DXGI_FORMAT_R32_FLOAT;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> sceneDepthsTexture;
	Graphics::Device->CreateTexture2D(&textureDesc, 0, sceneDepthsTexture.GetAddressOf());

	// Create the Render Target Views (null descriptions use default settings)
	Graphics::Device->CreateRenderTargetView(ppTexture.Get(), 0, ppRTV.GetAddressOf());
	Graphics::Device->CreateRenderTargetView(sceneNormalsTexture.Get(), 0, sceneNormalsRTV.GetAddressOf());
	Graphics::Device->CreateRenderTargetView(sceneDepthsTexture.Get(), 0, sceneDepthRTV.GetAddressOf());

	// Create the Shader Resource Views (null descriptions use default settings)
	Graphics::Device->CreateShaderResourceView(ppTexture.Get(), 0, ppSRV.GetAddressOf());
	Graphics::Device->CreateShaderResourceView(sceneNormalsTexture.Get(), 0, sceneNormalsSRV.GetAddressOf());
	Graphics::Device->CreateShaderResourceView(sceneDepthsTexture.Get(), 0, sceneDepthSRV.GetAddressOf());

	// Save for the UI, too
	options.SceneDepthsSRV = sceneDepthSRV;
	options.SceneNormalsSRV = sceneNormalsSRV;
}

// --------------------------------------------------------
// Clears buffers and sets up render targets
// --------------------------------------------------------
void Game::PreRender()
{
	// Background color for clearing
	const float color[4] = { 0, 0, 0, 1 };

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	Graphics::Context->ClearRenderTargetView(Graphics::BackBufferRTV.Get(), color);
	Graphics::Context->ClearDepthStencilView(Graphics::DepthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Clear all render targets, too
	Graphics::Context->ClearRenderTargetView(ppRTV.Get(), color);
	Graphics::Context->ClearRenderTargetView(sceneNormalsRTV.Get(), color);
	Graphics::Context->ClearRenderTargetView(sceneDepthRTV.Get(), color);

	// Assume three render targets (since pixel shader is always returning 3 numbers)
	ID3D11RenderTargetView* rtvs[3] =
	{
		Graphics::BackBufferRTV.Get(),
		sceneNormalsRTV.Get(),
		sceneDepthRTV.Get()
	};

	// Swap to the post process target if we need it
	if (options.OutlineMode != OutlineType::OutlineNone && 
		options.OutlineMode != OutlineType::OutlineInsideOut)
		rtvs[0] = ppRTV.Get();

	// Set all three
	Graphics::Context->OMSetRenderTargets(3, rtvs, Graphics::DepthBufferDSV.Get());
}


// --------------------------------------------------------
// Applies post processing if necessary
// --------------------------------------------------------
void Game::PostRender()
{
	// Which form of outline are we handling?
	switch (options.OutlineMode)
	{

	case OutlineSobelFilter:
		// Now that the scene is rendered, swap to the back buffer
		Graphics::Context->OMSetRenderTargets(1, Graphics::BackBufferRTV.GetAddressOf(), 0);

		// Set up post process shaders
		fullscreenVS->SetShader();

		// Note: Probably needs a clamp sampler, too
		sobelFilterPS->SetShader();
		sobelFilterPS->SetShaderResourceView("pixels", ppSRV.Get());
		sobelFilterPS->SetSamplerState("samplerOptions", clampSampler.Get());
		sobelFilterPS->SetFloat("pixelWidth", 1.0f / Window::Width());
		sobelFilterPS->SetFloat("pixelHeight", 1.0f / Window::Height());
		sobelFilterPS->CopyAllBufferData();

		// Draw exactly 3 vertices, which the special post-process vertex shader will
		// "figure out" on the fly (resulting in our "full screen triangle")
		Graphics::Context->Draw(3, 0);

		break;


	case OutlineSilhouette:

		// Now that the scene is rendered, swap to the back buffer
		Graphics::Context->OMSetRenderTargets(1, Graphics::BackBufferRTV.GetAddressOf(), 0);
		
		// Set up post process shaders
		fullscreenVS->SetShader();

		silhouettePS->SetShaderResourceView("pixels", ppSRV.Get());
		silhouettePS->SetSamplerState("samplerOptions", clampSampler.Get());
		silhouettePS->SetShader();

		silhouettePS->SetFloat("pixelWidth", 1.0f / Window::Width());
		silhouettePS->SetFloat("pixelHeight", 1.0f / Window::Height());
		silhouettePS->CopyAllBufferData();

		// Draw exactly 3 vertices, which the special post-process vertex shader will
		// "figure out" on the fly (resulting in our "full screen triangle")
		Graphics::Context->Draw(3, 0);

		break;

	case OutlineDepthNormals:

		// Now that the scene is rendered, swap to the back buffer
		Graphics::Context->OMSetRenderTargets(1, Graphics::BackBufferRTV.GetAddressOf(), 0);

		// Set up post process shaders
		fullscreenVS->SetShader();

		depthNormalOutlinePS->SetShaderResourceView("pixels", ppSRV.Get());
		depthNormalOutlinePS->SetShaderResourceView("normals", sceneNormalsSRV.Get());
		depthNormalOutlinePS->SetShaderResourceView("depth", sceneDepthSRV.Get());
		depthNormalOutlinePS->SetSamplerState("samplerOptions", clampSampler.Get());
		depthNormalOutlinePS->SetShader();

		depthNormalOutlinePS->SetFloat("pixelWidth", 1.0f / Window::Width());
		depthNormalOutlinePS->SetFloat("pixelHeight", 1.0f / Window::Height());
		depthNormalOutlinePS->SetFloat("depthAdjust", 5.0f);
		depthNormalOutlinePS->SetFloat("normalAdjust", 5.0f);
		depthNormalOutlinePS->CopyAllBufferData();

		// Draw exactly 3 vertices, which the special post-process vertex shader will
		// "figure out" on the fly (resulting in our "full screen triangle")
		Graphics::Context->Draw(3, 0);

		break;
	}

	// Unbind shader resource views at the end of the frame,
	// since we'll be rendering into one of those textures
	// at the start of the next
	ID3D11ShaderResourceView* nullSRVs[128] = {};
	Graphics::Context->PSSetShaderResources(0, ARRAYSIZE(nullSRVs), nullSRVs);
}


// --------------------------------------------------------
// Renders a single entity inside out, using a vertex shader
// that moves each vertex along its normal
// --------------------------------------------------------
void Game::DrawOutlineInsideOut(std::shared_ptr<GameEntity> entity, std::shared_ptr<Camera> camera, float outlineSize)
{
	insideOutVS->SetShader();
	solidColorPS->SetShader();

	insideOutVS->SetMatrix4x4("world", entity->GetTransform()->GetWorldMatrix());
	insideOutVS->SetMatrix4x4("view", camera->GetView());
	insideOutVS->SetMatrix4x4("projection", camera->GetProjection());
	insideOutVS->SetFloat("outlineSize", outlineSize);
	insideOutVS->CopyAllBufferData();

	solidColorPS->SetFloat3("Color", XMFLOAT3(0, 0, 0));
	solidColorPS->CopyAllBufferData();

	// Set render states
	Graphics::Context->RSSetState(insideOutRasterState.Get());

	// Draw the mesh
	entity->GetMesh()->SetBuffersAndDraw();

	// Reset render states
	Graphics::Context->RSSetState(0);
}
