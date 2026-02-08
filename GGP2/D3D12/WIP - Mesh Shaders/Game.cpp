#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "BufferStructs.h"
#include "AssetPath.h"

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

	// Load shaders
	{
		// Read our compiled vertex shader code into a blob
		// - Essentially just "open the file and plop its contents here"
		D3DReadFileToBlob(FixPath(L"VertexShader.cso").c_str(), vertexShaderByteCode.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"PixelShader.cso").c_str(), pixelShaderByteCode.GetAddressOf());
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
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
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
}


// --------------------------------------------------------
// Creates the geometry we're going to draw
// --------------------------------------------------------
void Game::CreateGeometry()
{
	// Load textures
	unsigned int cobblestoneAlbedo = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/cobblestone_albedo.png").c_str());
	unsigned int cobblestoneNormals = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/cobblestone_normals.png").c_str());
	unsigned int cobblestoneRoughness = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/cobblestone_roughness.png").c_str());
	unsigned int cobblestoneMetal = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/cobblestone_metal.png").c_str());

	unsigned int bronzeAlbedo = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/bronze_albedo.png").c_str());
	unsigned int bronzeNormals = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/bronze_normals.png").c_str());
	unsigned int bronzeRoughness = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/bronze_roughness.png").c_str());
	unsigned int bronzeMetal = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/bronze_metal.png").c_str());

	unsigned int scratchedAlbedo = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/scratched_albedo.png").c_str());
	unsigned int scratchedNormals = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/scratched_normals.png").c_str());
	unsigned int scratchedRoughness = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/scratched_roughness.png").c_str());
	unsigned int scratchedMetal = Graphics::LoadTexture(FixPath(AssetPath + L"Textures/PBR/scratched_metal.png").c_str());

	// Create materials
	// Note: Samplers are handled by a single static sampler in the
	// root signature for this demo, rather than per-material
	std::shared_ptr<Material> cobbleMat = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	cobbleMat->SetAlbedoIndex(bronzeAlbedo);
	cobbleMat->SetNormalMapIndex(bronzeNormals);
	cobbleMat->SetRoughnessIndex(bronzeRoughness);
	cobbleMat->SetMetalnessIndex(bronzeMetal);

	std::shared_ptr<Material> bronzeMat = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	bronzeMat->SetAlbedoIndex(cobblestoneAlbedo);
	bronzeMat->SetNormalMapIndex(cobblestoneNormals);
	bronzeMat->SetRoughnessIndex(cobblestoneRoughness);
	bronzeMat->SetMetalnessIndex(cobblestoneMetal);

	std::shared_ptr<Material> scratchedMat = std::make_shared<Material>(pipelineState, XMFLOAT3(1, 1, 1));
	scratchedMat->SetAlbedoIndex(scratchedAlbedo);
	scratchedMat->SetNormalMapIndex(scratchedNormals);
	scratchedMat->SetRoughnessIndex(scratchedRoughness);
	scratchedMat->SetMetalnessIndex(scratchedMetal);

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
	}

	// Rendering here!
	{
		// Set overall pipeline state
		Graphics::CommandList->SetPipelineState(pipelineState.Get());

		// Set constant buffer descriptor heap
		Graphics::CommandList->SetDescriptorHeaps(1, Graphics::CBVSRVDescriptorHeap.GetAddressOf());

		// Root sig
		Graphics::CommandList->SetGraphicsRootSignature(rootSignature.Get());



		// Set up other commands for rendering
		Graphics::CommandList->OMSetRenderTargets(1, &Graphics::RTVHandles[Graphics::SwapChainIndex()], true, &Graphics::DSVHandle);
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
		Graphics::AdvanceSwapChainIndex();

		// Wait for the GPU to be done and then reset the command list & allocator
		Graphics::WaitForGPU();
		Graphics::ResetAllocatorAndCommandList();
	}
}



