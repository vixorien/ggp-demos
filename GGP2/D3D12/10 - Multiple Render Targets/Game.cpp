#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "BufferStructs.h"
#include "AssetPath.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx12.h"
#include "ImGui/imgui_impl_win32.h"

#include <stdlib.h>     // For seeding random and rand()
#include <time.h>       // For grabbing time (to seed random)
#include <DirectXMath.h>

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

	// Seed random
	srand((unsigned int)time(0));

	lightCount = 16;
	GenerateLights();

	CreateRootSigAndPipelineState();
	CreateGeometry();

	// Create the camera
	camera = std::make_shared<FPSCamera>(
		XMFLOAT3(0.0f, 0.0f, -10.0f),	// Position
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
	// Wait for the GPU before we shut down
	Graphics::WaitForGPU();

	// ImGui clean up
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}


// --------------------------------------------------------
// Loads the two basic shaders, then creates the root signature 
// and pipeline state object for our very basic demo.
// --------------------------------------------------------
void Game::CreateRootSigAndPipelineState()
{
	// Blobs to hold raw shader byte code used in several steps below
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderByteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderByteCode;

	Microsoft::WRL::ComPtr<ID3DBlob> fullscreenVS;
	Microsoft::WRL::ComPtr<ID3DBlob> simpleTexturePS;

	// Load shaders
	{
		// Read our compiled vertex shader code into a blob
		// - Essentially just "open the file and plop its contents here"
		D3DReadFileToBlob(FixPath(L"VertexShader.cso").c_str(), vertexShaderByteCode.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"PixelShader.cso").c_str(), pixelShaderByteCode.GetAddressOf());

		D3DReadFileToBlob(FixPath(L"FullscreenVS.cso").c_str(), fullscreenVS.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"SimpleTexturePS.cso").c_str(), simpleTexturePS.GetAddressOf());
	}

	// Root Signature
	{
		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[1] = {};

		// Root params for descriptor indices
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[0].Constants.Num32BitValues = sizeof(DrawDescriptorIndices) / sizeof(unsigned int);
		rootParams[0].Constants.RegisterSpace = 0;
		rootParams[0].Constants.ShaderRegister = 0;

		// Create a single static sampler (available to all pixel shaders at the same slot)
		// Note: This is in lieu of having materials have their own samplers for this demo
		D3D12_STATIC_SAMPLER_DESC anisoWrap = {};
		anisoWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.Filter = D3D12_FILTER_ANISOTROPIC;
		anisoWrap.MaxAnisotropy = 16;
		anisoWrap.MaxLOD = D3D12_FLOAT32_MAX;
		anisoWrap.ShaderRegister = 0;  // register(s0)
		anisoWrap.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		D3D12_STATIC_SAMPLER_DESC samplers[] = { anisoWrap };

		// Describe and serialize the root signature
		D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
		rootSig.NumParameters = ARRAYSIZE(rootParams);
		rootSig.pParameters = rootParams;
		rootSig.NumStaticSamplers = ARRAYSIZE(samplers);
		rootSig.pStaticSamplers = samplers;

		ID3DBlob* serializedRootSig = 0;
		ID3DBlob* errors = 0;

		D3D12SerializeRootSignature(
			&rootSig,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&serializedRootSig,
			&errors);

		// Check for errors during serialization
		if (errors != 0)
		{
			OutputDebugString((wchar_t*)errors->GetBufferPointer());
		}

		// Actually create the root sig
		Graphics::Device->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(rootSignature.GetAddressOf()));
	}

	// Pipeline state
	{
		// Describe the pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

		// -- Input assembler related ---
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		// Overall primitive topology type (triangle, line, etc.) is set here 
		// IASetPrimTop() is still used to set list/strip/adj options
		// See: https://docs.microsoft.com/en-us/windows/desktop/direct3d12/managing-graphics-pipeline-state-in-direct3d-12

		// Root sig
		psoDesc.pRootSignature = rootSignature.Get();

		// -- Shaders (VS/PS) --- 
		psoDesc.VS.pShaderBytecode = vertexShaderByteCode->GetBufferPointer();
		psoDesc.VS.BytecodeLength = vertexShaderByteCode->GetBufferSize();
		psoDesc.PS.pShaderBytecode = pixelShaderByteCode->GetBufferPointer();
		psoDesc.PS.BytecodeLength = pixelShaderByteCode->GetBufferSize();

		// -- Render targets ---
		psoDesc.NumRenderTargets = 4;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.RTVFormats[3] = DXGI_FORMAT_R32_FLOAT;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		// -- States ---
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.RasterizerState.DepthClipEnable = true;

		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// -- Misc ---
		psoDesc.SampleMask = 0xffffffff;

		// Create the pipe state object
		Graphics::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf()));

		// ============================================
		// Also create the "full screen texture" PSO
		// - Assuming the same root sig is compatible

		psoDesc.PS.BytecodeLength = simpleTexturePS->GetBufferSize();
		psoDesc.PS.pShaderBytecode = simpleTexturePS->GetBufferPointer();

		psoDesc.VS.BytecodeLength = fullscreenVS->GetBufferSize();
		psoDesc.VS.pShaderBytecode = fullscreenVS->GetBufferPointer();

		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN; // Means "not used" here
		psoDesc.RTVFormats[2] = DXGI_FORMAT_UNKNOWN;
		psoDesc.RTVFormats[3] = DXGI_FORMAT_UNKNOWN;
		Graphics::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(fullScreenTexturePSO.GetAddressOf()));

	}

	// Set up the viewport and scissor rectangle
	{
		// Set up the viewport so we render into the correct
		// portion of the render target
		viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)Window::Width();
		viewport.Height = (float)Window::Height();
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		// Define a scissor rectangle that defines a portion of
		// the render target for clipping.  This is different from
		// a viewport in that it is applied after the pixel shader.
		// We need at least one of these, but we're rendering to 
		// the entire window, so it'll be the same size.
		scissorRect = {};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = Window::Width();
		scissorRect.bottom = Window::Height();
	}

	// Create RTV heap for render targets
	D3D12_DESCRIPTOR_HEAP_DESC dhDesc{};
	dhDesc.NumDescriptors = MaxRenderTargets;
	dhDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	Graphics::Device->CreateDescriptorHeap(&dhDesc, IID_PPV_ARGS(rtvDescriptorHeap.GetAddressOf()));

	D3D12_CPU_DESCRIPTOR_HANDLE rtv_cpu_start = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	unsigned int descSize = Graphics::Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Create render targets (note that depth uses R32 format!)
	RenderTargets[ColorRT] = Graphics::CreateTexture(Window::Width(), Window::Height(), 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	RenderTargets[NormalRT] = Graphics::CreateTexture(Window::Width(), Window::Height(), 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	RenderTargets[MaterialRT] = Graphics::CreateTexture(Window::Width(), Window::Height(), 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	RenderTargets[DepthRT] = Graphics::CreateTexture(Window::Width(), Window::Height(), 1, 1, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, DXGI_FORMAT_R32_FLOAT, 1, 0, 0, 0);

	// Update the texture details to point to contiguous spots in the RTV heap
	for (unsigned int i = 0; i < 4; i++)
	{
		RenderTargets[i].RTV = rtv_cpu_start;
		RenderTargets[i].RTV.ptr += descSize * i;
	}

	// Create RTVs for render targets
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Create the RTVs next to each other in the heap
	Graphics::Device->CreateRenderTargetView(RenderTargets[ColorRT].Texture.Get(), &rtvDesc, RenderTargets[ColorRT].RTV);
	Graphics::Device->CreateRenderTargetView(RenderTargets[NormalRT].Texture.Get(), &rtvDesc, RenderTargets[NormalRT].RTV);
	Graphics::Device->CreateRenderTargetView(RenderTargets[MaterialRT].Texture.Get(), &rtvDesc, RenderTargets[MaterialRT].RTV);

	// Depth needs different format
	rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	Graphics::Device->CreateRenderTargetView(RenderTargets[DepthRT].Texture.Get(), &rtvDesc, RenderTargets[DepthRT].RTV);

	// Create SRVs, too
	D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
	srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv.Texture2D.MipLevels = 1;
	srv.Texture2D.MostDetailedMip = 0;
	srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Reserve 4 SRVs
	for (unsigned int i = 0; i < 4; i++)
		Graphics::ReserveDescriptorHeapSlot(&RenderTargets[i].SRV.CPUHandle, &RenderTargets[i].SRV.GPUHandle);
	
	// Create
	Graphics::Device->CreateShaderResourceView(RenderTargets[ColorRT].Texture.Get(), &srv, RenderTargets[ColorRT].SRV.CPUHandle);
	Graphics::Device->CreateShaderResourceView(RenderTargets[NormalRT].Texture.Get(), &srv, RenderTargets[NormalRT].SRV.CPUHandle);
	Graphics::Device->CreateShaderResourceView(RenderTargets[MaterialRT].Texture.Get(), &srv, RenderTargets[MaterialRT].SRV.CPUHandle);

	// Depth has different format
	srv.Format = DXGI_FORMAT_R32_FLOAT;
	Graphics::Device->CreateShaderResourceView(RenderTargets[DepthRT].Texture.Get(), &srv, RenderTargets[DepthRT].SRV.CPUHandle);

	// Update SRV indices
	for (unsigned int i = 0; i < 4; i++)
		RenderTargets[i].SRV.GPUDescriptorIndex = Graphics::GetDescriptorIndex(RenderTargets[i].SRV.GPUHandle);
}


// --------------------------------------------------------
// Creates the geometry we're going to draw
// --------------------------------------------------------
void Game::CreateGeometry()
{
	// Load textures
	TextureDetails cobblestoneAlbedo = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/cobblestone_albedo.png").c_str());
	TextureDetails cobblestoneNormals = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/cobblestone_normals.png").c_str());
	TextureDetails cobblestoneRoughness = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/cobblestone_roughness.png").c_str());
	TextureDetails cobblestoneMetal = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/cobblestone_metal.png").c_str());

	TextureDetails bronzeAlbedo = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/bronze_albedo.png").c_str());
	TextureDetails bronzeNormals = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/bronze_normals.png").c_str());
	TextureDetails bronzeRoughness = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/bronze_roughness.png").c_str());
	TextureDetails bronzeMetal = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/bronze_metal.png").c_str());

	TextureDetails scratchedAlbedo = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/scratched_albedo.png").c_str());
	TextureDetails scratchedNormals = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/scratched_normals.png").c_str());
	TextureDetails scratchedRoughness = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/scratched_roughness.png").c_str());
	TextureDetails scratchedMetal = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/scratched_metal.png").c_str());

	// Create materials
	// Note: Samplers are handled by a single static sampler in the
	// root signature for this demo, rather than per-material
	std::shared_ptr<Material> cobbleMat = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	cobbleMat->SetAlbedoIndex(bronzeAlbedo.SRV.GPUDescriptorIndex);
	cobbleMat->SetNormalMapIndex(bronzeNormals.SRV.GPUDescriptorIndex);
	cobbleMat->SetRoughnessIndex(bronzeRoughness.SRV.GPUDescriptorIndex);
	cobbleMat->SetMetalnessIndex(bronzeMetal.SRV.GPUDescriptorIndex);

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	bronzeMat->SetAlbedoIndex(cobblestoneAlbedo.SRV.GPUDescriptorIndex);
	bronzeMat->SetNormalMapIndex(cobblestoneNormals.SRV.GPUDescriptorIndex);
	bronzeMat->SetRoughnessIndex(cobblestoneRoughness.SRV.GPUDescriptorIndex);
	bronzeMat->SetMetalnessIndex(cobblestoneMetal.SRV.GPUDescriptorIndex);

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	scratchedMat->SetAlbedoIndex(scratchedAlbedo.SRV.GPUDescriptorIndex);
	scratchedMat->SetNormalMapIndex(scratchedNormals.SRV.GPUDescriptorIndex);
	scratchedMat->SetRoughnessIndex(scratchedRoughness.SRV.GPUDescriptorIndex);
	scratchedMat->SetMetalnessIndex(scratchedMetal.SRV.GPUDescriptorIndex);

	// Load meshes
	std::shared_ptr<Mesh> cube = std::make_shared<Mesh>("Cube", FixPath(AssetPath + L"Meshes/cube.obj").c_str());
	std::shared_ptr<Mesh> sphere = std::make_shared<Mesh>("Sphere", FixPath(AssetPath + L"Meshes/sphere.obj").c_str());
	std::shared_ptr<Mesh> helix = std::make_shared<Mesh>("Helix", FixPath(AssetPath + L"Meshes/helix.obj").c_str());
	std::shared_ptr<Mesh> torus = std::make_shared<Mesh>("Torus", FixPath(AssetPath + L"Meshes/torus.obj").c_str());
	std::shared_ptr<Mesh> cylinder = std::make_shared<Mesh>("Cylinder", FixPath(AssetPath + L"Meshes/cylinder.obj").c_str());

	// Create entities
	std::shared_ptr<GameEntity> entityCube = std::make_shared<GameEntity>(cube, scratchedMat);
	entityCube->GetTransform()->SetPosition(3, 0, 0);

	std::shared_ptr<GameEntity> entityHelix = std::make_shared<GameEntity>(helix, cobbleMat);
	entityHelix->GetTransform()->SetPosition(0, 0, 0);

	std::shared_ptr<GameEntity> entitySphere = std::make_shared<GameEntity>(sphere, bronzeMat);
	entitySphere->GetTransform()->SetPosition(-3, 0, 0);

	// Add to list
	entities.push_back(entityCube);
	entities.push_back(entityHelix);
	entities.push_back(entitySphere);

	// Load the sky
	sky = std::make_shared<Sky>(
		FixPath(AssetPath + L"Skies/Clouds Blue/right.png").c_str(),
		FixPath(AssetPath + L"Skies/Clouds Blue/left.png").c_str(),
		FixPath(AssetPath + L"Skies/Clouds Blue/up.png").c_str(),
		FixPath(AssetPath + L"Skies/Clouds Blue/down.png").c_str(),
		FixPath(AssetPath + L"Skies/Clouds Blue/front.png").c_str(),
		FixPath(AssetPath + L"Skies/Clouds Blue/back.png").c_str(),
		cube);
}


// --------------------------------------------------------
// Generates (or regenerates) lights for the scene
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
	// Resize the viewport and scissor rectangle
	{
		// Set up the viewport so we render into the correct
		// portion of the render target
		viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = (float)Window::Width();
		viewport.Height = (float)Window::Height();
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		// Define a scissor rectangle that defines a portion of
		// the render target for clipping.  This is different from
		// a viewport in that it is applied after the pixel shader.
		// We need at least one of these, but we're rendering to 
		// the entire window, so it'll be the same size.
		scissorRect = {};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = Window::Width();
		scissorRect.bottom = Window::Height();
	}

	// Update the camera's projection to match the new size
	if (camera)
	{
		camera->UpdateProjectionMatrix(Window::AspectRatio());
	}
}


// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Set up the new frame for ImGui, then build this frame's UI
	UINewFrame(deltaTime);
	BuildUI();

	// Example input checking: Quit if the escape key is pressed
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();

	camera->Update(deltaTime);

	for (auto& e : entities)
		e->GetTransform()->Rotate(0, deltaTime, 0);
}


// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Grab the current back buffer for this frame
	Microsoft::WRL::ComPtr<ID3D12Resource> currentBackBuffer = Graphics::BackBuffers[Graphics::SwapChainIndex()];

	// Clearing the render target
	{
		// Transition the back buffer from present to render target
		D3D12_RESOURCE_BARRIER rb = {};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		rb.Transition.pResource = currentBackBuffer.Get();
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		Graphics::CommandList->ResourceBarrier(1, &rb);

		// Background color for clearing
		float color[] = { 0,0,0,1 };

		// Clear the RTV
		Graphics::CommandList->ClearRenderTargetView(
			Graphics::RTVHandles[Graphics::SwapChainIndex()],
			color,
			0, 0); // No scissor rectangles

		// Clear the depth buffer, too
		Graphics::CommandList->ClearDepthStencilView(
			Graphics::DSVHandle,
			D3D12_CLEAR_FLAG_DEPTH,
			1.0f,	// Max depth = 1.0f
			0,		// Not clearing stencil, but need a value
			0, 0);	// No scissor rects

		// Transition RTs and clear, too
		float depthClear[4] = { 1,0,0,0 };
		for (unsigned int i = 0; i < 4; i++)
		{
			rb.Transition.pResource = RenderTargets[i].Texture.Get();
			rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			rb.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			Graphics::CommandList->ResourceBarrier(1, &rb);

			if (i == DepthRT)
				Graphics::CommandList->ClearRenderTargetView(RenderTargets[i].RTV, depthClear, 0, 0);
			else
				Graphics::CommandList->ClearRenderTargetView(RenderTargets[i].RTV, color, 0, 0);
		}
	}

	// Rendering here!
	{
		// Set overall pipeline state
		Graphics::CommandList->SetPipelineState(pipelineState.Get());

		// Set constant buffer descriptor heap
		Graphics::CommandList->SetDescriptorHeaps(1, Graphics::CBVSRVDescriptorHeap.GetAddressOf());

		// Root sig
		Graphics::CommandList->SetGraphicsRootSignature(rootSignature.Get());

		// Set multiple render targets
		Graphics::CommandList->OMSetRenderTargets(
			4,                             // Set 4 at once
			&RenderTargets[ColorRT].RTV,  // Address of first
			true,                          // True means all 4 are next to each other
			&Graphics::DSVHandle);         // Depth buffer DSV

		// Set up other commands for rendering
		Graphics::CommandList->RSSetViewports(1, &viewport);
		Graphics::CommandList->RSSetScissorRects(1, &scissorRect);
		Graphics::CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Set up per-frame data
		DrawDescriptorIndices drawData{};
		
		// Per-frame vertex data
		{
			VertexShaderPerFrameData vsFrame{};
			vsFrame.view = camera->GetView();
			vsFrame.projection = camera->GetProjection();

			D3D12_GPU_DESCRIPTOR_HANDLE cbHandleVS = Graphics::FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&vsFrame), sizeof(VertexShaderPerFrameData));

			drawData.vsPerFrameCBIndex = Graphics::GetDescriptorIndex(cbHandleVS);
		}

		// Per-frame pixel data
		{
			PixelShaderPerFrameData psFrame{};
			psFrame.cameraPosition = camera->GetTransform()->GetPosition();
			psFrame.lightCount = lightCount;
			memcpy(psFrame.lights, &lights[0], sizeof(Light) * lightCount);

			D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = Graphics::FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&psFrame), sizeof(PixelShaderPerFrameData));

			drawData.psPerFrameCBIndex = Graphics::GetDescriptorIndex(cbHandlePS);
		}

		// Loop through the entities
		for (auto& e : entities)
		{
			// Grab the material for this entity
			std::shared_ptr<Material> mat = e->GetMaterial();

			// Set the pipeline state for this material
			{
				Graphics::CommandList->SetPipelineState(mat->GetPipelineState().Get());
			}

			drawData.vsVertexBufferIndex = Graphics::GetDescriptorIndex(e->GetMesh()->GetVertexBufferDescriptorHandle());

			// Set up the data we intend to use for drawing this entity
			{
				VertexShaderPerObjectData vsData = {};
				vsData.world = e->GetTransform()->GetWorldMatrix();
				vsData.worldInverseTranspose = e->GetTransform()->GetWorldInverseTransposeMatrix();

				// Send this to a chunk of the constant buffer heap
				// and grab the GPU handle for it so we can set it for this draw
				D3D12_GPU_DESCRIPTOR_HANDLE cbHandleVS = Graphics::FillNextConstantBufferAndGetGPUDescriptorHandle(
					(void*)(&vsData), sizeof(VertexShaderPerObjectData));

				drawData.vsPerObjectCBIndex = Graphics::GetDescriptorIndex(cbHandleVS);
			}

			// Pixel shader data and cbuffer setup
			{
				PixelShaderPerObjectData psData = {};
				psData.uvScale = mat->GetUVScale();
				psData.uvOffset = mat->GetUVOffset();
				psData.albedoIndex = mat->GetAlbedoIndex();
				psData.normalMapIndex = mat->GetNormalMapIndex();
				psData.roughnessIndex = mat->GetRoughnessIndex();
				psData.metalnessIndex = mat->GetMetalnessIndex();

				// Send this to a chunk of the constant buffer heap
				// and grab the GPU handle for it so we can set it for this draw
				D3D12_GPU_DESCRIPTOR_HANDLE cbHandlePS = Graphics::FillNextConstantBufferAndGetGPUDescriptorHandle(
					(void*)(&psData), sizeof(PixelShaderPerObjectData));

				drawData.psPerObjectCBIndex = Graphics::GetDescriptorIndex(cbHandlePS);
			}

			Graphics::CommandList->SetGraphicsRoot32BitConstants(
				0, 
				sizeof(DrawDescriptorIndices) / sizeof(unsigned int), 
				&drawData,
				0);

			// Grab the mesh and its buffer views
			std::shared_ptr<Mesh> mesh = e->GetMesh();
			D3D12_INDEX_BUFFER_VIEW  ibv = mesh->GetIndexBufferView();

			// Set the geometry
			Graphics::CommandList->IASetIndexBuffer(&ibv);

			// Draw
			Graphics::CommandList->DrawIndexedInstanced((UINT)mesh->GetIndexCount(), 1, 0, 0, 0);
		}
	}

	// Skybox after opaque objects
	sky->Draw(camera);

	// Back to back buffer
	Graphics::CommandList->OMSetRenderTargets(1, &Graphics::RTVHandles[Graphics::SwapChainIndex()], true, &Graphics::DSVHandle);

	// Transition RTs to pixel shader resources
	for (unsigned int i = 0; i < 4; i++)
	{
		D3D12_RESOURCE_BARRIER rb = {};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		rb.Transition.pResource = RenderTargets[i].Texture.Get();
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		Graphics::CommandList->ResourceBarrier(1, &rb);
	}

	// Put the pixels on the screen
	{
		Graphics::CommandList->SetPipelineState(fullScreenTexturePSO.Get());
		Graphics::CommandList->SetGraphicsRootSignature(rootSignature.Get());

		// Set index of 1 texture
		Graphics::CommandList->SetGraphicsRoot32BitConstants(0, 1, &RenderTargets[ColorRT].SRV.GPUDescriptorIndex, 0);

		Graphics::CommandList->DrawInstanced(3, 1, 0, 0);
	}

	// ImGui Render after all other scene objects
	{
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Graphics::CommandList.Get());
	}

	// Present
	{
		// Transition back to present
		D3D12_RESOURCE_BARRIER rb = {};
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		rb.Transition.pResource = currentBackBuffer.Get();
		rb.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		Graphics::CommandList->ResourceBarrier(1, &rb);

		// Must occur BEFORE present
		Graphics::CloseAndExecuteCommandList();

		// Present the current back buffer and move to the next one
		bool vsync = Graphics::VsyncState();
		Graphics::SwapChain->Present(
			vsync ? 1 : 0,
			vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING);

		// Reset the command list & allocator for the upcoming frame
		Graphics::AdvanceSwapChainIndex();
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

		// === Multiple Render Targets ===
		if (ImGui::TreeNode("Render Targets"))
		{
			float width = ImGui::GetWindowWidth();
			ImVec2 size = ImVec2(
				width,
				width / Window::AspectRatio());

			for (unsigned int i = 0; i < 4; i++)
			{
				// Convert descriptor index BACK into actual GPU handle
				ImageWithHover(RenderTargets[i].SRV.GPUHandle, size);
			}

			// Finalize the tree node
			ImGui::TreePop();
		}


	}
	ImGui::End();
}

void Game::ImageWithHover(D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle, const ImVec2& size)
{
	// Draw the image
	ImGui::Image(ImTextureRef(gpuDescHandle.ptr), size);

	// Check for hover
	if (ImGui::IsItemHovered())
	{
		// Zoom amount and aspect of the image
		float zoom = 0.03f;
		float aspect = (float)size.x / size.y;

		// Get the coords of the image
		ImVec2 topLeft = ImGui::GetItemRectMin();
		ImVec2 bottomRight = ImGui::GetItemRectMax();

		// Get the mouse pos as a percent across the image, clamping near the edge
		ImVec2 mousePosGlobal = ImGui::GetMousePos();
		ImVec2 mousePos = ImVec2(mousePosGlobal.x - topLeft.x, mousePosGlobal.y - topLeft.y);
		ImVec2 uvPercent = ImVec2(mousePos.x / size.x, mousePos.y / size.y);

		uvPercent.x = max(uvPercent.x, zoom / 2);
		uvPercent.x = min(uvPercent.x, 1 - zoom / 2);
		uvPercent.y = max(uvPercent.y, zoom / 2 * aspect);
		uvPercent.y = min(uvPercent.y, 1 - zoom / 2 * aspect);

		// Figure out the uv coords for the zoomed image
		ImVec2 uvTL = ImVec2(uvPercent.x - zoom / 2, uvPercent.y - zoom / 2 * aspect);
		ImVec2 uvBR = ImVec2(uvPercent.x + zoom / 2, uvPercent.y + zoom / 2 * aspect);

		// Draw a floating box with a zoomed view of the image
		ImGui::BeginTooltip();
		ImGui::Image(ImTextureRef(gpuDescHandle.ptr), ImVec2(256, 256), uvTL, uvBR);
		ImGui::EndTooltip();
	}
}







