#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "BufferStructs.h"
#include "AssetPath.h"
#include "RayTracing.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx12.h"
#include "ImGui/imgui_impl_win32.h"

#include <DirectXMath.h>
#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// Helper macro for getting a float between min and max
#define RandomRange(min, max) (float)rand() / RAND_MAX * (max - min) + min

// --------------------------------------------------------
// Called once per program, the window and graphics API
// are initialized but before the game loop begins
// --------------------------------------------------------
void Game::Initialize()
{
	// Seed random
	srand((unsigned int)time(0));

	// Reserve a descriptor slot for ImGui's font texture
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	Graphics::ReserveDescriptorHeapSlot(&cpuHandle, &gpuHandle);

	// Initialize ImGui itself & platform/renderer backends
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(Window::Handle());
	{
		// Note: Recent change in how we feed ImGui initial D3D12 details!
		ImGui_ImplDX12_InitInfo info{};
		info.CommandQueue = Graphics::CommandQueue.Get();
		info.Device = Graphics::Device.Get();
		info.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		info.LegacySingleSrvCpuDescriptor = cpuHandle;
		info.LegacySingleSrvGpuDescriptor = gpuHandle;
		info.NumFramesInFlight = Graphics::NumBackBuffers;
		info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		info.SrvDescriptorHeap = Graphics::CBVSRVDescriptorHeap.Get();

		ImGui_ImplDX12_Init(&info);
	}
	
	// Create the camera
	camera = std::make_shared<FPSCamera>(
		XMFLOAT3(0.0f, 8.0f, -20.0f),	// Position
		5.0f,					// Move speed
		0.002f,					// Look speed
		XM_PIDIV4,				// Field of view
		Window::AspectRatio(),  // Aspect ratio
		0.01f,					// Near clip
		100.0f,					// Far clip
		CameraProjectionType::Perspective);

	// Initialize raytracing
	RayTracing::Initialize(
		Window::Width(),
		Window::Height(),
		FixPath(L"RayTracing.cso"));

	// Load the skybox
	skyboxHandle = Graphics::LoadCubeTexture(
		FixPath(AssetPath + L"/Skies/Clouds Blue/right.png").c_str(),
		FixPath(AssetPath + L"/Skies/Clouds Blue/left.png").c_str(),
		FixPath(AssetPath + L"/Skies/Clouds Blue/up.png").c_str(),
		FixPath(AssetPath + L"/Skies/Clouds Blue/down.png").c_str(),
		FixPath(AssetPath + L"/Skies/Clouds Blue/front.png").c_str(),
		FixPath(AssetPath + L"/Skies/Clouds Blue/back.png").c_str());


	// Quick macro to simplify texture loading lines below
#define LoadTexture(x) Graphics::LoadTexture(FixPath(x).c_str())

	// Load textures
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneAlbedo = LoadTexture(AssetPath + L"Textures/PBR/cobblestone_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneNormals = LoadTexture(AssetPath + L"Textures/PBR/cobblestone_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneRoughness = LoadTexture(AssetPath + L"Textures/PBR/cobblestone_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE cobblestoneMetal = LoadTexture(AssetPath + L"Textures/PBR/cobblestone_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE bronzeAlbedo = LoadTexture(AssetPath + L"Textures/PBR/bronze_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeNormals = LoadTexture(AssetPath + L"Textures/PBR/bronze_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeRoughness = LoadTexture(AssetPath + L"Textures/PBR/bronze_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE bronzeMetal = LoadTexture(AssetPath + L"Textures/PBR/bronze_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE scratchedAlbedo = LoadTexture(AssetPath + L"Textures/PBR/scratched_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedNormals = LoadTexture(AssetPath + L"Textures/PBR/scratched_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedRoughness = LoadTexture(AssetPath + L"Textures/PBR/scratched_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE scratchedMetal = LoadTexture(AssetPath + L"Textures/PBR/scratched_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE woodAlbedo = LoadTexture(AssetPath + L"Textures/PBR/wood_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE woodNormals = LoadTexture(AssetPath + L"Textures/PBR/wood_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE woodRoughness = LoadTexture(AssetPath + L"Textures/PBR/wood_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE woodMetal = LoadTexture(AssetPath + L"Textures/PBR/wood_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE floorAlbedo = LoadTexture(AssetPath + L"Textures/PBR/floor_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE floorNormals = LoadTexture(AssetPath + L"Textures/PBR/floor_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE floorRoughness = LoadTexture(AssetPath + L"Textures/PBR/floor_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE floorMetal = LoadTexture(AssetPath + L"Textures/PBR/floor_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE paintAlbedo = LoadTexture(AssetPath + L"Textures/PBR/paint_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE paintNormals = LoadTexture(AssetPath + L"Textures/PBR/paint_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE paintRoughness = LoadTexture(AssetPath + L"Textures/PBR/paint_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE paintMetal = LoadTexture(AssetPath + L"Textures/PBR/paint_metal.png");

	D3D12_CPU_DESCRIPTOR_HANDLE ironAlbedo = LoadTexture(AssetPath + L"Textures/PBR/rough_albedo.png");
	D3D12_CPU_DESCRIPTOR_HANDLE ironNormals = LoadTexture(AssetPath + L"Textures/PBR/rough_normals.png");
	D3D12_CPU_DESCRIPTOR_HANDLE ironRoughness = LoadTexture(AssetPath + L"Textures/PBR/rough_roughness.png");
	D3D12_CPU_DESCRIPTOR_HANDLE ironMetal = LoadTexture(AssetPath + L"Textures/PBR/rough_metal.png");

	// Create materials
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState{};
	// Create materials
	// Note: Samplers are handled by a single static sampler in the
	// root signature for this demo, rather than per-material
	std::shared_ptr<Material> greyDiffuse = std::make_shared<Material>(pipelineState, XMFLOAT3(0.5f, 0.5f, 0.5f), 1.0f, 0.0f);
	std::shared_ptr<Material> darkGrey = std::make_shared<Material>(pipelineState, XMFLOAT3(0.25f, 0.25f, 0.25f), 0.0f, 1.0f);
	std::shared_ptr<Material> metal = std::make_shared<Material>(pipelineState, XMFLOAT3(0.5f, 0.6f, 0.7f), 0.0f, 1.0f);

	std::shared_ptr<Material> cobblestone = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> scratched = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> bronze = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> floor = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> paint = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> iron = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> wood = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));

	// Set up textures
	cobblestone->AddTexture(cobblestoneAlbedo, 0);
	cobblestone->AddTexture(cobblestoneNormals, 1);
	cobblestone->AddTexture(cobblestoneRoughness, 2);
	cobblestone->AddTexture(cobblestoneMetal, 3);
	cobblestone->FinalizeTextures();

	scratched->AddTexture(scratchedAlbedo, 0);
	scratched->AddTexture(scratchedNormals, 1);
	scratched->AddTexture(scratchedRoughness, 2);
	scratched->AddTexture(scratchedMetal, 3);
	scratched->FinalizeTextures();

	bronze->AddTexture(bronzeAlbedo, 0);
	bronze->AddTexture(bronzeNormals, 1);
	bronze->AddTexture(bronzeRoughness, 2);
	bronze->AddTexture(bronzeMetal, 3);
	bronze->FinalizeTextures();

	floor->AddTexture(floorAlbedo, 0);
	floor->AddTexture(floorNormals, 1);
	floor->AddTexture(floorRoughness, 2);
	floor->AddTexture(floorMetal, 3);
	floor->FinalizeTextures();

	paint->AddTexture(paintAlbedo, 0);
	paint->AddTexture(paintNormals, 1);
	paint->AddTexture(paintRoughness, 2);
	paint->AddTexture(paintMetal, 3);
	paint->FinalizeTextures();

	wood->AddTexture(woodAlbedo, 0);
	wood->AddTexture(woodNormals, 1);
	wood->AddTexture(woodRoughness, 2);
	wood->AddTexture(woodMetal, 3);
	wood->FinalizeTextures();

	iron->AddTexture(ironAlbedo, 0);
	iron->AddTexture(ironNormals, 1);
	iron->AddTexture(ironRoughness, 2);
	iron->AddTexture(ironMetal, 3);
	iron->FinalizeTextures();

	// Load mesh(es)
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>("Cube", FixPath(AssetPath + L"Meshes/cube.obj").c_str());
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>("Torus", FixPath(AssetPath + L"Meshes/torus.obj").c_str()); 
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>("Sphere", FixPath(AssetPath + L"Meshes/sphere.obj").c_str());

	// Floor
	std::shared_ptr<GameEntity> ground = std::make_shared<GameEntity>(cubeMesh, wood);
	ground->GetTransform()->SetScale(100);
	ground->GetTransform()->SetPosition(0, -101, 0);
	entities.push_back(ground);

	// Spinning torus
	std::shared_ptr<GameEntity> t = std::make_shared<GameEntity>(torusMesh, metal);
	t->GetTransform()->SetScale(4);
	t->GetTransform()->SetPosition(0, 10, 0);
	entities.push_back(t);

	float range = 20;
	for (int i = 0; i < 50; i++)
	{
		// Random roughness either 0 or 1
		float rough = RandomRange(0.0f, 1.0f) > 0.5f ? 0.0f : 1.0f;
		float metalness = RandomRange(0.0f, 1.0f) > 0.5f ? 0.0f : 1.0f;

		// Randomly choose some others
		std::shared_ptr<Material> mat;
		float randMat = RandomRange(0, 1);
		if (randMat > 0.95f) mat = bronze;
		else if (randMat > 0.9f) mat = cobblestone;
		else if (randMat > 0.85f) mat = scratched;
		else if (randMat > 0.8f) mat = wood;
		else if (randMat > 0.75f) mat = iron;
		else if (randMat > 0.7f) mat = paint;
		else if (randMat > 0.65f) mat = floor;
		else mat = std::make_shared<Material>(
				pipelineState,
				XMFLOAT3(
					RandomRange(0.0f, 1.0f),
					RandomRange(0.0f, 1.0f),
					RandomRange(0.0f, 1.0f)),
				rough,
				metalness);

		// Create the sphere
		std::shared_ptr<GameEntity> sphereEnt = std::make_shared<GameEntity>(sphereMesh, mat);
		entities.push_back(sphereEnt);

		// Randomly scale and position
		float scale = RandomRange(0.5f, 3.5f);
		sphereEnt->GetTransform()->SetScale(scale);
		sphereEnt->GetTransform()->SetPosition(
			RandomRange(-range, range),
			scale - 1,
			RandomRange(-range, range));

	}



	// Once we have all of the BLAS ready, we can make a TLAS
	RayTracing::CreateTopLevelAccelerationStructureForScene(entities);

	// Finalize any initialization and wait for the GPU
	// before proceeding to the game loop
	Graphics::CloseAndExecuteCommandList();
	Graphics::WaitForGPU();
	Graphics::ResetAllocatorAndCommandList(0);

}


// --------------------------------------------------------
// Clean up memory or objects created by this class
// 
// Note: Using smart pointers means there probably won't
//       be much to manually clean up here!
// --------------------------------------------------------
Game::~Game()
{
	// Wait for the GPU before we shut down
	Graphics::WaitForGPU();

	// ImGui clean up
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

// --------------------------------------------------------
// Handle resizing to match the new window size
//  - Eventually, we'll want to update our 3D camera
// --------------------------------------------------------
void Game::OnResize()
{
	// Update the camera's projection to match the new size
	if (camera)
		camera->UpdateProjectionMatrix(Window::AspectRatio());

	// Resize raytracing output texture
	RayTracing::ResizeOutputUAV(Window::Width(), Window::Height());
}


// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Example input checking: Quit if the escape key is pressed
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();
	
	// Set up the new frame for ImGui, then build this frame's UI
	UINewFrame(deltaTime);
	BuildUI();

	// Handle cam input
	camera->Update(deltaTime);
	
	// Rotate the torus
	entities[1]->GetTransform()->Rotate(deltaTime * 0.5f, deltaTime * 0.5f, deltaTime * 0.5f);

	// Rotate entities (skip first two)
	float range = 40;
	int skip = 2;
	float scaledTime = totalTime * 2.0f;
	for (int i = skip; i < entities.size(); i++)
	{
		XMFLOAT3 pos = entities[i]->GetTransform()->GetPosition();
		XMFLOAT3 rot = entities[i]->GetTransform()->GetPitchYawRoll();
		XMFLOAT3 sc = entities[i]->GetTransform()->GetScale();
		switch (i % 2)
		{
		case 0:
			pos.x = sin((scaledTime + i) * (4 / range)) * range;
			rot.z = -pos.x / (sc.x);
			break;

		case 1:
			pos.z = sin((scaledTime + i) * (4 / range)) * range;
			rot.x = pos.z / (sc.x);
			break;
		}
		entities[i]->GetTransform()->SetPosition(pos);
		entities[i]->GetTransform()->SetRotation(rot);

	}

}


// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Grab the current back buffer for this frame
	Microsoft::WRL::ComPtr<ID3D12Resource> currentBackBuffer = Graphics::BackBuffers[Graphics::SwapChainIndex()];

	// Prepare a resoruce barrier for various transitions below
	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	rb.Transition.pResource = currentBackBuffer.Get();
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// Raytracing: Update the TLAS for the latest entity positions and then trace
	{
		RayTracing::CreateTopLevelAccelerationStructureForScene(entities);
		RayTracing::Raytrace(camera, currentBackBuffer, skyboxHandle);
	}

	// ImGui Render after all other scene objects
	{
		// The raytracing call above assumes we'll be presenting immediately afterwards,
		// which leaves the back buffer in the PRESENT state.  We'll need to transition
		// back to RENDER_TARGET so that ImGui can also render.  This is definitely
		// an extra step, and could be generalized by not automatically transitioning
		// to PRESENT at the end of raytracing.
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		Graphics::CommandList->ResourceBarrier(1, &rb);

		// ImGui needs the descriptor heap (where its font texture lives) and the render target
		Graphics::CommandList->SetDescriptorHeaps(1, Graphics::CBVSRVDescriptorHeap.GetAddressOf());
		Graphics::CommandList->OMSetRenderTargets(1, &Graphics::RTVHandles[Graphics::SwapChainIndex()], true, 0);

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Graphics::CommandList.Get());
	}

	// Present
	{
		// Transition back to present
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		Graphics::CommandList->ResourceBarrier(1, &rb);

		// Must occur BEFORE present
		Graphics::CloseAndExecuteCommandList();

		// Present the current back buffer and move to the next one
		bool vsync = Graphics::VsyncState();
		Graphics::SwapChain->Present(
			vsync ? 1 : 0,
			vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING);
		Graphics::AdvanceSwapChainIndex();

		// Reset the command list & allocator for the upcoming frame
		Graphics::ResetAllocatorAndCommandList(Graphics::SwapChainIndex());
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
	io.DisplaySize.x = (float)Window::Width();
	io.DisplaySize.y = (float)Window::Height();

	// Reset the frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Determine new input capture
	Input::SetKeyboardCapture(io.WantCaptureKeyboard);
	Input::SetMouseCapture(io.WantCaptureMouse);
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
			ImGui::Text("Window Client Size: %dx%d", Window::Width(), Window::Height());

			// Should we show the demo window?
			if (ImGui::Button(showUIDemoWindow ? "Hide ImGui Demo Window" : "Show ImGui Demo Window"))
				showUIDemoWindow = !showUIDemoWindow;

			ImGui::Spacing();

			// Finalize the tree node
			ImGui::TreePop();
		}
	}
	ImGui::End();
}


