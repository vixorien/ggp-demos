#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "BufferStructs.h"
#include "AssetPath.h"

#include <DirectXMath.h>

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// For the DirectX Math library
using namespace DirectX;

// --------------------------------------------------------
// Called once per program, the window and graphics API
// are initialized but before the game loop begins
// --------------------------------------------------------
void Game::Initialize()
{
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

	// Input layout
	const unsigned int inputElementCount = 4;
	D3D12_INPUT_ELEMENT_DESC inputElements[inputElementCount] = {};
	{
		// Create an input layout that describes the vertex format
		// used by the vertex shader we're using
		//  - This is used by the pipeline to know how to interpret the raw data
		//     sitting inside a vertex buffer

		// Set up the first element - a position, which is 3 float values
		inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT; // How far into the vertex is this?  Assume it's after the previous element
		inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// Most formats are described as color channels, really it just means "Three 32-bit floats"
		inputElements[0].SemanticName = "POSITION";					// This is "POSITTION" - needs to match the semantics in our vertex shader input!
		inputElements[0].SemanticIndex = 0;							// This is the 0th position (there could be more)

		// Set up the second element - a UV, which is 2 more float values
		inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;			// 2x 32-bit floats
		inputElements[1].SemanticName = "TEXCOORD";					// Match our vertex shader input!
		inputElements[1].SemanticIndex = 0;							// This is the 0th uv (there could be more)

		// Set up the third element - a normal, which is 3 more float values
		inputElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// 3x 32-bit floats
		inputElements[2].SemanticName = "NORMAL";					// Match our vertex shader input!
		inputElements[2].SemanticIndex = 0;							// This is the 0th normal (there could be more)

		// Set up the fourth element - a tangent, which is 2 more float values
		inputElements[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;	// After the previous element
		inputElements[3].Format = DXGI_FORMAT_R32G32B32_FLOAT;		// 3x 32-bit floats
		inputElements[3].SemanticName = "TANGENT";					// Match our vertex shader input!
		inputElements[3].SemanticIndex = 0;							// This is the 0th tangent (there could be more)
	}

	// Root Signature
	{
		// Define a table of CBV's (constant buffer views)
		D3D12_DESCRIPTOR_RANGE cbvTable = {};
		cbvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		cbvTable.NumDescriptors = 1;
		cbvTable.BaseShaderRegister = 0;
		cbvTable.RegisterSpace = 0;
		cbvTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Define the root parameter
		D3D12_ROOT_PARAMETER rootParam = {};
		rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParam.DescriptorTable.NumDescriptorRanges = 1;
		rootParam.DescriptorTable.pDescriptorRanges = &cbvTable;

		// Describe and serialize the root signature
		D3D12_ROOT_SIGNATURE_DESC rootSig = {};
		rootSig.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSig.NumParameters = 1;
		rootSig.pParameters = &rootParam;
		rootSig.NumStaticSamplers = 0;
		rootSig.pStaticSamplers = 0;

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
		psoDesc.InputLayout.NumElements = inputElementCount;
		psoDesc.InputLayout.pInputElementDescs = inputElements;
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
	// Load meshes
	std::shared_ptr<Mesh> cube = std::make_shared<Mesh>("Cube", FixPath(AssetPath + L"Meshes/cube.obj").c_str());
	std::shared_ptr<Mesh> sphere = std::make_shared<Mesh>("Sphere", FixPath(AssetPath + L"Meshes/sphere.obj").c_str());
	std::shared_ptr<Mesh> helix = std::make_shared<Mesh>("Helix", FixPath(AssetPath + L"Meshes/helix.obj").c_str());
	std::shared_ptr<Mesh> torus = std::make_shared<Mesh>("Torus", FixPath(AssetPath + L"Meshes/torus.obj").c_str());
	std::shared_ptr<Mesh> cylinder = std::make_shared<Mesh>("Cylinder", FixPath(AssetPath + L"Meshes/cylinder.obj").c_str());

	// Array for cycling through meshes below
	std::shared_ptr<Mesh> meshes[5] = { cube, sphere, helix, torus, cylinder };

	// Create a critical mass of entity to tax the frame rate a bit
	int meshCount = 0;
	int dimensionSize = 12;
	int halfSize = dimensionSize / 2;
	for (float x = 0; x < dimensionSize; x++)
	{
		for (float y = 0; y < dimensionSize; y++)
		{
			for (float z = 0; z < dimensionSize; z++)
			{
				// Create a single entity
				std::shared_ptr<GameEntity> e = std::make_shared<GameEntity>(meshes[meshCount % 5]);
				e->GetTransform()->SetPosition(x - halfSize, y - halfSize, z);
				e->GetTransform()->SetScale(0.25f);
				entities.push_back(e);

				meshCount++;
			}
		}
	}
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

		// Background color (Cornflower Blue in this case) for clearing
		float color[] = { 0.4f, 0.6f, 0.75f, 1.0f };

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

		// Root sig (must happen before root descriptor table)
		Graphics::CommandList->SetGraphicsRootSignature(rootSignature.Get());


		// Set constant buffer descriptor heap
		Graphics::CommandList->SetDescriptorHeaps(1, Graphics::CBVSRVDescriptorHeap.GetAddressOf());

		// Set up other commands for rendering
		Graphics::CommandList->OMSetRenderTargets(1, &Graphics::RTVHandles[Graphics::SwapChainIndex()], true, &Graphics::DSVHandle);
		Graphics::CommandList->RSSetViewports(1, &viewport);
		Graphics::CommandList->RSSetScissorRects(1, &scissorRect);
		Graphics::CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Loop through the entities
		for (auto& e : entities)
		{
			// Set up the data we intend to use for drawing this entity
			VertexShaderExternalData vsData = {};
			vsData.world = e->GetTransform()->GetWorldMatrix();
			vsData.view = camera->GetView();
			vsData.projection = camera->GetProjection();

			// Send this to a chunk of the constant buffer heap
			// and grab the GPU handle for it so we can set it for this draw
			D3D12_GPU_DESCRIPTOR_HANDLE cbHandle = Graphics::FillNextConstantBufferAndGetGPUDescriptorHandle(
				(void*)(&vsData), sizeof(VertexShaderExternalData));

			// Set this constant buffer handle
			// Note: This assumes that descriptor table 0 is the
			//       place to put this particular descriptor.  This
			//       is based on how we set up our root signature.
			Graphics::CommandList->SetGraphicsRootDescriptorTable(0, cbHandle);

			// Grab the mesh and its buffer views
			std::shared_ptr<Mesh> mesh = e->GetMesh();
			D3D12_VERTEX_BUFFER_VIEW vbv = mesh->GetVertexBufferView();
			D3D12_INDEX_BUFFER_VIEW  ibv = mesh->GetIndexBufferView();

			// Set the geometry
			Graphics::CommandList->IASetVertexBuffers(0, 1, &vbv);
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

		// Reset the command list & allocator for the upcoming frame
		Graphics::AdvanceSwapChainIndex();
		Graphics::ResetAllocatorAndCommandList(Graphics::SwapChainIndex());
	}
}



