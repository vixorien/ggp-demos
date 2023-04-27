#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "Assets.h"
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
	drawLights(true),
	freezeLightMovement(false),
	lightCount(3),
	pauseMovement(false),
	movementTime(0.0f),
	parallaxHeightScale(0.0f),
	parallaxSamples(256),
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

	// Deleting the singleton reference we've set up here
	delete& Assets::GetInstance();

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

	// Loading scene stuff
	LoadAssetsAndCreateEntities();

	// Set up lights
	lightCount = 3;
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
	// Initialize the asset manager and set it to load assets on demand
	Assets& assets = Assets::GetInstance();
	assets.Initialize(L"../../../Assets/", L"./", device, context, true, true);

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
	sky = std::make_shared<Sky>(
		FixPath(L"../../../Assets/Skies/Clouds Blue/right.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/left.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/up.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/down.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/front.png").c_str(),
		FixPath(L"../../../Assets/Skies/Clouds Blue/back.png").c_str(),
		assets.GetMesh(L"Models/cube"),
		assets.GetVertexShader(L"SkyVS"),
		assets.GetPixelShader(L"SkyPS"),
		sampler,
		device,
		context);


	// Grab shaders needed below
	std::shared_ptr<SimpleVertexShader> vertexShader = assets.GetVertexShader(L"VertexShader");
	std::shared_ptr<SimplePixelShader> pixelShader = assets.GetPixelShader(L"PixelShaderPBR");

	std::shared_ptr<Material> parallaxShapesMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	parallaxShapesMat->AddSampler("BasicSampler", sampler);
	parallaxShapesMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/wood_albedo"));
	parallaxShapesMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/shapes_normals"));
	parallaxShapesMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/wood_roughness"));
	parallaxShapesMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/wood_metal"));
	parallaxShapesMat->AddTextureSRV("HeightMap", assets.GetTexture(L"Textures/shapes_height"));

	std::shared_ptr<Material> parallaxStonesMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	parallaxStonesMat->AddSampler("BasicSampler", sampler);
	parallaxStonesMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/stones"));
	parallaxStonesMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/stones_normals"));
	parallaxStonesMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/stones_height"));
	parallaxStonesMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/wood_metal")); // White
	parallaxStonesMat->AddTextureSRV("HeightMap", assets.GetTexture(L"Textures/stones_height"));

	std::shared_ptr<Material> parallaxLeatherMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	parallaxLeatherMat->AddSampler("BasicSampler", sampler);
	parallaxLeatherMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/leather_albedo"));
	parallaxLeatherMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/leather_normals"));
	parallaxLeatherMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/leather_rough"));
	parallaxLeatherMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/leather_metal"));
	parallaxLeatherMat->AddTextureSRV("HeightMap", assets.GetTexture(L"Textures/PBR/leather_height"));

	std::shared_ptr<Material> parallaxBricksMat = std::make_shared<Material>(pixelShader, vertexShader, XMFLOAT3(1, 1, 1), XMFLOAT2(1, 1));
	parallaxBricksMat->AddSampler("BasicSampler", sampler);
	parallaxBricksMat->AddTextureSRV("Albedo", assets.GetTexture(L"Textures/PBR/bricks_albedo"));
	parallaxBricksMat->AddTextureSRV("NormalMap", assets.GetTexture(L"Textures/PBR/bricks_normals"));
	parallaxBricksMat->AddTextureSRV("RoughnessMap", assets.GetTexture(L"Textures/PBR/bricks_rough"));
	parallaxBricksMat->AddTextureSRV("MetalMap", assets.GetTexture(L"Textures/PBR/bricks_metal"));
	parallaxBricksMat->AddTextureSRV("HeightMap", assets.GetTexture(L"Textures/PBR/bricks_height"));

	// === Create the scene ===
	std::shared_ptr<GameEntity> shapesCube = std::make_shared<GameEntity>(assets.GetMesh(L"Models/cube"), parallaxShapesMat);
	shapesCube->GetTransform()->SetScale(3);
	shapesCube->GetTransform()->SetPosition(0, 0, 0);
	entities.push_back(shapesCube);

	std::shared_ptr<GameEntity> leatherCube = std::make_shared<GameEntity>(assets.GetMesh(L"Models/cube"), parallaxLeatherMat);
	leatherCube->GetTransform()->SetScale(3);
	leatherCube->GetTransform()->SetPosition(-5, 0, 0);
	entities.push_back(leatherCube);

	std::shared_ptr<GameEntity> bricksCube = std::make_shared<GameEntity>(assets.GetMesh(L"Models/cube"), parallaxBricksMat);
	bricksCube->GetTransform()->SetScale(3);
	bricksCube->GetTransform()->SetPosition(5, 0, 0);
	entities.push_back(bricksCube);

	std::shared_ptr<GameEntity> plane = std::make_shared<GameEntity>(assets.GetMesh(L"Models/quad_double_sided"), parallaxStonesMat);
	plane->GetTransform()->SetScale(2);
	plane->GetTransform()->SetPosition(0, -5, 0);
	plane->GetTransform()->SetRotation(-XM_PIDIV2, 0, 0);
	entities.push_back(plane);
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

	entities[0]->GetTransform()->SetRotation(0, movementTime * 0.1f, 0);

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
	for (auto& e : entities)
	{
		std::shared_ptr<SimplePixelShader> ps = e->GetMaterial()->GetPixelShader();
		ps->SetFloat3("ambientColor", ambientColor);
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetInt("lightCount", lightCount);
		ps->SetFloat("heightScale", parallaxHeightScale);
		ps->SetInt("parallaxSamples", parallaxSamples);

		// Draw one entity
		e->Draw(context, camera);
	}

	// Draw the sky after all regular entities
	sky->Draw(camera);

	// Draw the light sources
	if(drawLights)
		DrawLightSources();

	// We need to un-bind (deactivate) the shadow map as a 
	// shader resource since we'll be using it as a depth buffer
	// at the beginning of next frame!  To make it easy, I'm simply
	// unbinding all SRV's from pixel shader stage here
	ID3D11ShaderResourceView* nullSRVs[16] = {};
	context->PSSetShaderResources(0, 16, nullSRVs);

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
	Assets& assets = Assets::GetInstance();
	std::shared_ptr<Mesh> lightMesh = assets.GetMesh(L"Models/sphere");
	std::shared_ptr<SimpleVertexShader> vs = assets.GetVertexShader(L"VertexShader");
	std::shared_ptr<SimplePixelShader> ps = assets.GetPixelShader(L"SolidColorPS");

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

			// Finalize the tree node
			ImGui::TreePop();
		}

		// === Parallax ===
		if (ImGui::TreeNode("Parallax Mapping"))
		{

			ImGui::Spacing();
			ImGui::SliderFloat("Height Scale", &parallaxHeightScale, 0.0f, 1.0f);
			ImGui::SliderInt("Number of Samples", &parallaxSamples, 16, 512);

			ImVec2 size = ImGui::GetItemRectSize();
			ImGui::Spacing();
			ImGui::Text("Example Height Map");
			ImGui::Image(Assets::GetInstance().GetTexture(L"Textures/shapes_height").Get(), ImVec2(size.x, size.x));

			// Finalize the tree node
			ImGui::TreePop();
		}

		ImGui::Checkbox("Pause Rotation", &pauseMovement);

		ImGui::End();
	}
}

