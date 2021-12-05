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
	drawLights(true),
	freezeLightMovement(false),
	lightCount(3),
	pauseMovement(false),
	movementTime(0.0f)
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
	for (auto& e : entities) delete e;

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

	// Set up shadow mapping resources
	CreateShadowMapResources();
}


// --------------------------------------------------------
// Loads all necessary assets and creates various entities
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Initialize the asset manager and set it to load assets on demand
	Assets& assets = Assets::GetInstance();
	assets.Initialize("../../../Assets/", device, context, true, true);

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
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/right.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/left.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/up.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/down.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/front.png").c_str(),
		GetFullPathTo_Wide(L"../../../Assets/Skies/Clouds Blue/back.png").c_str(),
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
	std::shared_ptr<Material> cobbleMat2x = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	cobbleMat2x->AddSampler("BasicSampler", sampler);
	cobbleMat2x->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/cobblestone_albedo"));
	cobbleMat2x->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/cobblestone_normals"));
	cobbleMat2x->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/cobblestone_roughness"));
	cobbleMat2x->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/cobblestone_metal"));
	
	std::shared_ptr<Material> cobbleMat4x = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 4));
	cobbleMat4x->AddSampler("BasicSampler", sampler);
	cobbleMat4x->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/cobblestone_albedo"));
	cobbleMat4x->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/cobblestone_normals"));
	cobbleMat4x->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/cobblestone_roughness"));
	cobbleMat4x->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/cobblestone_metal"));

	std::shared_ptr<Material> floorMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	floorMat->AddSampler("BasicSampler", sampler);
	floorMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/floor_albedo"));
	floorMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/floor_normals"));
	floorMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/floor_roughness"));
	floorMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/floor_metal"));

	std::shared_ptr<Material> paintMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	paintMat->AddSampler("BasicSampler", sampler);
	paintMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/paint_albedo"));
	paintMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/paint_normals"));
	paintMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/paint_roughness"));
	paintMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/paint_metal"));

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	scratchedMat->AddSampler("BasicSampler", sampler);
	scratchedMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/scratched_albedo"));
	scratchedMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/scratched_normals"));
	scratchedMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/scratched_roughness"));
	scratchedMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/scratched_metal"));

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	bronzeMat->AddSampler("BasicSampler", sampler);
	bronzeMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/bronze_albedo"));
	bronzeMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/bronze_normals"));
	bronzeMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/bronze_roughness"));
	bronzeMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/bronze_metal"));

	std::shared_ptr<Material> roughMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(4, 2));
	roughMat->AddSampler("BasicSampler", sampler);
	roughMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/rough_albedo"));
	roughMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/rough_normals"));
	roughMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/rough_roughness"));
	roughMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/rough_metal"));

	std::shared_ptr<Material> woodMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	woodMat->AddSampler("BasicSampler", sampler);
	woodMat->AddTextureSRV("Albedo", assets.GetTexture("Textures/PBR/wood_albedo"));
	woodMat->AddTextureSRV("NormalMap", assets.GetTexture("Textures/PBR/wood_normals"));
	woodMat->AddTextureSRV("RoughnessMap", assets.GetTexture("Textures/PBR/wood_roughness"));
	woodMat->AddTextureSRV("MetalMap", assets.GetTexture("Textures/PBR/wood_metal"));


	// === Create the scene ===
	GameEntity* floor = new GameEntity(assets.GetMesh("Models/cube"), cobbleMat4x);
	floor->GetTransform()->SetScale(50, 50, 50);
	floor->GetTransform()->SetPosition(0, -27, 0);
	entities.push_back(floor);

	GameEntity* sphere = new GameEntity(assets.GetMesh("Models/sphere"), scratchedMat);
	sphere->GetTransform()->SetPosition(-5, 0, 0);
	entities.push_back(sphere);

	GameEntity* helix = new GameEntity(assets.GetMesh("Models/helix"), paintMat);
	entities.push_back(helix);

	GameEntity* cube = new GameEntity(assets.GetMesh("Models/cube"), woodMat);
	cube->GetTransform()->SetPosition(5, 0, 0);
	cube->GetTransform()->SetScale(2, 2, 2);
	entities.push_back(cube);

	GameEntity* hoverSphere = new GameEntity(assets.GetMesh("Models/sphere"), paintMat);
	hoverSphere->GetTransform()->SetScale(2.5f, 2.5f, 2.5f);
	hoverSphere->GetTransform()->SetPosition(0, 5, -5);
	entities.push_back(hoverSphere);
}


void Game::CreateShadowMapResources()
{
	// Create shadow requirements ------------------------------------------
	shadowMapResolution = 1024;
	shadowProjectionSize = 10.0f;

	// Create the actual texture that will be the shadow map
	D3D11_TEXTURE2D_DESC shadowDesc = {};
	shadowDesc.Width = shadowMapResolution;
	shadowDesc.Height = shadowMapResolution;
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
	device->CreateTexture2D(&shadowDesc, 0, shadowTexture.GetAddressOf());

	// Create the depth/stencil
	D3D11_DEPTH_STENCIL_VIEW_DESC shadowDSDesc = {};
	shadowDSDesc.Format = DXGI_FORMAT_D32_FLOAT;
	shadowDSDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	shadowDSDesc.Texture2D.MipSlice = 0;
	device->CreateDepthStencilView(shadowTexture.Get(), &shadowDSDesc, shadowDSV.GetAddressOf());

	// Create the SRV for the shadow map
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	device->CreateShaderResourceView(shadowTexture.Get(), &srvDesc, shadowSRV.GetAddressOf());

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
	device->CreateSamplerState(&shadowSampDesc, &shadowSampler);

	// Create a rasterizer state
	D3D11_RASTERIZER_DESC shadowRastDesc = {};
	shadowRastDesc.FillMode = D3D11_FILL_SOLID;
	shadowRastDesc.CullMode = D3D11_CULL_BACK;
	shadowRastDesc.DepthClipEnable = true;
	shadowRastDesc.DepthBias = 1000; // Multiplied by (smallest possible positive value storable in the depth buffer)
	shadowRastDesc.DepthBiasClamp = 0.0f;
	shadowRastDesc.SlopeScaledDepthBias = 1.0f;
	device->CreateRasterizerState(&shadowRastDesc, &shadowRasterizer);

	// Create the "camera" matrices for the shadow map rendering

	// View
	XMMATRIX shView = XMMatrixLookAtLH(
		XMVectorSet(0, 20, -20, 0),
		XMVectorSet(0, 0, 0, 0),
		XMVectorSet(0, 1, 0, 0));
	XMStoreFloat4x4(&shadowViewMatrix, shView);

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
	XMMATRIX shProj = XMMatrixOrthographicLH(shadowProjectionSize, shadowProjectionSize, 0.1f, 100.0f);
	XMStoreFloat4x4(&shadowProjectionMatrix, shProj);

}

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

	// Check individual input
	if (input.KeyPress(VK_TAB)) pauseMovement = !pauseMovement;
	if (input.KeyPress('F')) freezeLightMovement = !freezeLightMovement;
	if (input.KeyPress('L')) drawLights = !drawLights;

	// Handle light count changes, clamped appropriately
	if (input.KeyDown('R')) lightCount = 3;
	if (input.KeyDown(VK_UP)) lightCount++;
	if (input.KeyDown(VK_DOWN)) lightCount--;
	lightCount = max(1, min(MAX_LIGHTS, lightCount));

	// Shadow map adjustments
	bool projChanged = false;
	if (input.KeyDown(VK_LEFT)) { shadowProjectionSize -= deltaTime * 5.0f; projChanged = true; }
	if (input.KeyDown(VK_RIGHT)) { shadowProjectionSize += deltaTime * 5.0f; projChanged = true; }
	if (projChanged)
	{
		// Clamp the size and then re-create the shadow "camera" projection
		shadowProjectionSize = max(0.1f, shadowProjectionSize);
		XMMATRIX shProj = XMMatrixOrthographicLH(shadowProjectionSize, shadowProjectionSize, 0.1f, 100.0f);
		XMStoreFloat4x4(&shadowProjectionMatrix, shProj);
	}

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

	// Move entities
	if (!pauseMovement)
		movementTime += deltaTime;

	// First three moving entities move up and down
	float height = sin(movementTime) * 2.0f;
	entities[1]->GetTransform()->SetPosition(-5, height, 0);
	entities[2]->GetTransform()->SetPosition(0, height, 0);
	entities[3]->GetTransform()->SetPosition(5, height, 0);

	// Fourth moves side to side
	entities[4]->GetTransform()->SetPosition(sin(movementTime * 2) * 8.0f, 5, -5);
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

	// Render the shadow map before rendering anything to the screen
	RenderShadowMap();

	// Loop through the game entities in the current scene and draw
	for (auto& e : entities)
	{
		std::shared_ptr<SimpleVertexShader> vs = e->GetMaterial()->GetVertexShader();
		vs->SetMatrix4x4("shadowView", shadowViewMatrix);
		vs->SetMatrix4x4("shadowProjection", shadowProjectionMatrix);

		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("lightCount", lightCount);

		ps->SetShaderResourceView("ShadowMap", shadowSRV);
		ps->SetSamplerState("ShadowSampler", shadowSampler);

		// Draw one entity
		e->Draw(context, camera);
	}

	// Draw the sky after all regular entities
	sky->Draw(camera);

	// Draw the light sources
	if(drawLights)
		DrawLightSources();

	// Draw the UI on top of everything
	DrawUI();

	// We need to un-bind (deactivate) the shadow map as a 
	// shader resource since we'll be using it as a depth buffer
	// at the beginning of next frame!  To make it easy, I'm simply
	// unbinding all SRV's from pixel shader stage here
	ID3D11ShaderResourceView* nullSRVs[16] = {};
	context->PSSetShaderResources(0, 16, nullSRVs);

	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);

	// Due to the usage of a more sophisticated swap chain,
	// the render target must be re-bound after every call to Present()
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthStencilView.Get());
}



// -------------------------------------------------------
// Renders the shadow map from the light's point of view
// -------------------------------------------------------
void Game::RenderShadowMap()
{
	// Initial pipeline setup - No RTV necessary - Clear shadow map
	context->OMSetRenderTargets(0, 0, shadowDSV.Get());
	context->ClearDepthStencilView(shadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	context->RSSetState(shadowRasterizer.Get());

	// Need to create a viewport that matches the shadow map resolution
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)shadowMapResolution;
	viewport.Height = (float)shadowMapResolution;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	context->RSSetViewports(1, &viewport);

	// Turn on our shadow map Vertex Shader
	// and turn OFF the pixel shader entirely
	std::shared_ptr<SimpleVertexShader> shadowVS = Assets::GetInstance().GetVertexShader("ShadowVS");
	shadowVS->SetShader();
	shadowVS->SetMatrix4x4("view", shadowViewMatrix);
	shadowVS->SetMatrix4x4("projection", shadowProjectionMatrix);
	context->PSSetShader(0, 0, 0); // No PS

	// Loop and draw all entities
	for (auto& e : entities)
	{
		shadowVS->SetMatrix4x4("world", e->GetTransform()->GetWorldMatrix());
		shadowVS->CopyAllBufferData();

		// Draw the mesh
		e->GetMesh()->SetBuffersAndDraw(context);
	}

	// After rendering the shadow map, go back to the screen
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthStencilView.Get());
	viewport.Width = (float)this->width;
	viewport.Height = (float)this->height;
	context->RSSetViewports(1, &viewport);
	context->RSSetState(0);
}



void Game::DrawLightSources()
{
	Assets& assets = Assets::GetInstance();
	std::shared_ptr<Mesh> lightMesh = assets.GetMesh("Models/sphere");
	std::shared_ptr<SimpleVertexShader> vs = assets.GetVertexShader("VertexShader");
	std::shared_ptr<SimplePixelShader> ps = assets.GetPixelShader("SolidColorPS");

	// Turn on the light mesh
	Microsoft::WRL::ComPtr<ID3D11Buffer> vb = lightMesh->GetVertexBuffer();
	Microsoft::WRL::ComPtr<ID3D11Buffer> ib = lightMesh->GetIndexBuffer();
	unsigned int indexCount = lightMesh->GetIndexCount();

	// Turn on these shaders
	vs->SetShader();
	ps->SetShader();

	// Set up vertex shader
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());

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
		vs->SetMatrix4x4("world", world);

		// Set up the pixel shader data
		XMFLOAT3 finalColor = light.Color;
		finalColor.x *= light.Intensity;
		finalColor.y *= light.Intensity;
		finalColor.z *= light.Intensity;
		ps->SetFloat3("Color", finalColor);

		// Copy data
		vs->CopyAllBufferData();
		ps->CopyAllBufferData();

		// Draw
		context->DrawIndexed(indexCount, 0, 0);
	}

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
	fontArial12->DrawString(spriteBatch.get(), L" (TAB) Freeze/unfreeze entities", XMVectorSet(10, h + 80, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (F) Freeze/unfreeze lights", XMVectorSet(10, h + 100, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (L) Show Point Lights:", XMVectorSet(10, h + 120, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), drawLights ? L"On" : L"Off", XMVectorSet(180, h + 120, 0, 0), drawLights ? XMVectorSet(0, 1, 0, 1) : XMVectorSet(1, 0, 0, 1));

	// Light count
	h = 180;
	fontArial12->DrawString(spriteBatch.get(), L"Light Count:", XMVectorSet(10, h, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), std::to_wstring(lightCount).c_str(), XMVectorSet(180, h, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (R) Reset Light Count", XMVectorSet(10, h + 20, 0, 0));

	// Shadows
	h = 260;
	fontArial12->DrawString(spriteBatch.get(), L"Shadows:", XMVectorSet(10, h, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" Shadow Map Resolution:", XMVectorSet(10, h + 20, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), std::to_wstring(shadowMapResolution).c_str(), XMVectorSet(220, h + 20, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" Shadow Projection Size:", XMVectorSet(10, h + 40, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), std::to_wstring(shadowProjectionSize).c_str(), XMVectorSet(220, h + 40, 0, 0));
	fontArial12->DrawString(spriteBatch.get(), L" (Arrow Left/Right) Change projection size", XMVectorSet(10, h + 60, 0, 0));


	spriteBatch->End();

	// Reset render states, since sprite batch changes these!
	context->OMSetBlendState(0, 0, 0xFFFFFFFF);
	context->OMSetDepthStencilState(0, 0);

}
