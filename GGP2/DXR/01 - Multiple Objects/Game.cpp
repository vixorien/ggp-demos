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
	// Check for DXR support
	if(FAILED(RayTracing::Initialize()))
		return;

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
		XMFLOAT3(0.0f, 2.0f, -10.0f),	// Position
		5.0f,					// Move speed
		0.002f,					// Look speed
		XM_PIDIV4,				// Field of view
		Window::AspectRatio(),  // Aspect ratio
		0.01f,					// Near clip
		100.0f,					// Far clip
		CameraProjectionType::Perspective);

	// Create materials
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState{};
	std::shared_ptr<Material> greyMat = std::make_shared<Material>(pipelineState, XMFLOAT3(0.5f, 0.5f, 0.5f));
	std::shared_ptr<Material> lightGreyMat = std::make_shared<Material>(pipelineState, XMFLOAT3(0.9f, 0.9f, 1));

	// Load mesh(es)
	std::shared_ptr<Mesh> cubeMesh = std::make_shared<Mesh>("Cube", FixPath(AssetPath + L"Meshes/cube.obj").c_str());
	std::shared_ptr<Mesh> torusMesh = std::make_shared<Mesh>("Torus", FixPath(AssetPath + L"Meshes/torus.obj").c_str()); 
	std::shared_ptr<Mesh> sphereMesh = std::make_shared<Mesh>("Sphere", FixPath(AssetPath + L"Meshes/sphere.obj").c_str());

	// Floor
	std::shared_ptr<GameEntity> floor = std::make_shared<GameEntity>(cubeMesh, greyMat);
	floor->GetTransform()->SetScale(50);
	floor->GetTransform()->SetPosition(0, -51, 0);
	entities.push_back(floor);

	// Spinning torus
	std::shared_ptr<GameEntity> t = std::make_shared<GameEntity>(torusMesh, lightGreyMat);
	t->GetTransform()->SetScale(2);
	t->GetTransform()->SetPosition(0, 3, 0);
	entities.push_back(t);

	for (int i = 0; i < 20; i++)
	{
		std::shared_ptr<Material> mat = std::make_shared<Material>(pipelineState, XMFLOAT3(
			RandomRange(0.0f, 1.0f),
			RandomRange(0.0f, 1.0f),
			RandomRange(0.0f, 1.0f)));

		float scale = RandomRange(0.25f, 1.0f);

		std::shared_ptr<GameEntity> sphereEnt = std::make_shared<GameEntity>(sphereMesh, mat);
		sphereEnt->GetTransform()->SetScale(scale);
		sphereEnt->GetTransform()->SetPosition(
			RandomRange(-6, 6),
			-1 + scale,
			RandomRange(-6, 6));

		entities.push_back(sphereEnt);
	}

	// Initialize raytracing
	RayTracing::CreateRequiredResources(
		Window::Width(),
		Window::Height(),
		FixPath(L"RayTracing.cso"),
		entities);


	// Note: Waiting until the first Draw() to build the
	// initial ray tracing top level accel structure
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

	// Move the sphere entities (skipping cube and torus)
	for (int i = 2; i < entities.size(); i++)
	{
		XMFLOAT3 pos = entities[i]->GetTransform()->GetPosition();
		switch (i % 2)
		{
		case 0:
			pos.x = sin((totalTime + i) * 0.4f) * 4;
			break;

		case 1:
			pos.z = sin((totalTime + i) * 0.4f) * 4;
			break;
		}
		entities[i]->GetTransform()->SetPosition(pos);
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
		RayTracing::Raytrace(camera, currentBackBuffer);
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


