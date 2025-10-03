#include "Game.h"
#include "Graphics.h"
#include "Vertex.h"
#include "Input.h"
#include "PathHelpers.h"
#include "Window.h"
#include "UIHelpers.h"
#include "AssetPath.h"
#include "BufferStructs.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

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
		// Set up a constant buffer heap of an appropriate size
		Graphics::ResizeConstantBufferHeap(256 * 1000); // 1000 chunks of 256 bytes

		// Tell the input assembler (IA) stage of the pipeline what kind of
		// geometric primitives (points, lines or triangles) we want to draw.  
		// Essentially: "What kind of shape should the GPU draw with our vertices?"
		Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Create an input layout 
		//  - This describes the layout of data sent to a vertex shader
		//  - In other words, it describes how to interpret data (numbers) in a vertex buffer
		//  - Doing this NOW because it requires a vertex shader's byte code to verify against!
		D3D11_INPUT_ELEMENT_DESC inputElements[3] = {};

		// Set up the first element - a position, which is 3 float values
		inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;				// Most formats are described as color channels; really it just means "Three 32-bit floats"
		inputElements[0].SemanticName = "POSITION";							// This is "POSITION" - needs to match the semantics in our vertex shader input!
		inputElements[0].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;	// How far into the vertex is this?  Assume it's after the previous element

		// Set up the second element - a color, which is 4 more float values
		inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;					// 2x 32-bit floats
		inputElements[1].SemanticName = "TEXCOORD";							// Match our vertex shader input!
		inputElements[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;	// After the previous element

		// Set up the second element - a color, which is 4 more float values
		inputElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;				// 3x 32-bit floats
		inputElements[2].SemanticName = "NORMAL";							// Match our vertex shader input!
		inputElements[2].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;	// After the previous element


		// Create the input layout, verifying our description against actual shader code
		ID3DBlob* vertexShaderBlob;
		D3DReadFileToBlob(FixPath(L"VertexShader.cso").c_str(), &vertexShaderBlob);
		Graphics::Device->CreateInputLayout(
			inputElements,							// An array of descriptions
			ARRAYSIZE(inputElements),				// How many elements in that array?
			vertexShaderBlob->GetBufferPointer(),	// Pointer to the code of a shader that uses this layout
			vertexShaderBlob->GetBufferSize(),		// Size of the shader code that uses this layout
			inputLayout.GetAddressOf());			// Address of the resulting ID3D11InputLayout pointer

		// Set the input layout now that it exists
		Graphics::Context->IASetInputLayout(inputLayout.Get());
	}

	// Create the camera
	camera = std::make_shared<FPSCamera>(
		XMFLOAT3(0.0f, 3.0f, -15.0f),	// Position
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
// Loads a pixel shader from a compiled shader object (.cso) file
// --------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11PixelShader> Game::LoadPixelShader(const wchar_t* compiledShaderPath)
{
	// Read the contents of the compiled shader object to a blob
	ID3DBlob* shaderBlob;
	D3DReadFileToBlob(compiledShaderPath, &shaderBlob);

	// Create the pixel shader and return it
	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader;
	Graphics::Device->CreatePixelShader(
		shaderBlob->GetBufferPointer(),	// Pointer to blob's contents
		shaderBlob->GetBufferSize(),	// How big is that data?
		0,								// No classes in this shader
		shader.GetAddressOf());			// Address of the ID3D11PixelShader pointer
	return shader;
}


// --------------------------------------------------------
// Loads a vertex shader from a compiled shader object (.cso) file
// --------------------------------------------------------
Microsoft::WRL::ComPtr<ID3D11VertexShader> Game::LoadVertexShader(const wchar_t* compiledShaderPath)
{
	// Read the contents of the compiled shader object to a blob
	ID3DBlob* shaderBlob;
	D3DReadFileToBlob(compiledShaderPath, &shaderBlob);

	// Create the pixel shader and return it
	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
	Graphics::Device->CreateVertexShader(
		shaderBlob->GetBufferPointer(),	// Pointer to blob's contents
		shaderBlob->GetBufferSize(),	// How big is that data?
		0,								// No classes in this shader
		shader.GetAddressOf());			// Address of the ID3D11PixelShader pointer
	return shader;
}


// --------------------------------------------------------
// Loads assets and creates the geometry we're going to draw
// --------------------------------------------------------
void Game::LoadAssetsAndCreateEntities()
{
	// Load shaders
	Microsoft::WRL::ComPtr<ID3D11VertexShader> basicVertexShader = LoadVertexShader(FixPath(L"VertexShader.cso").c_str());
	Microsoft::WRL::ComPtr<ID3D11PixelShader> basicPixelShader = LoadPixelShader(FixPath(L"PixelShader.cso").c_str());
	Microsoft::WRL::ComPtr<ID3D11PixelShader> fancyPixelShader = LoadPixelShader(FixPath(L"FancyPixelShader.cso").c_str());
	Microsoft::WRL::ComPtr<ID3D11PixelShader> normalPreviewPS = LoadPixelShader(FixPath(L"NormalPreviewPS.cso").c_str());
	Microsoft::WRL::ComPtr<ID3D11PixelShader> uvPreviewPS = LoadPixelShader(FixPath(L"UVPreviewPS.cso").c_str());

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

	// Create several different materials
	std::shared_ptr<Material> matFancy = std::make_shared<Material>("Fancy", fancyPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> matUV = std::make_shared<Material>("UV Preview", uvPreviewPS, basicVertexShader, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> matNorm = std::make_shared<Material>("Normal Preview", normalPreviewPS, basicVertexShader, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> matWhite = std::make_shared<Material>("Solid White", basicPixelShader, basicVertexShader, XMFLOAT3(1, 1, 1));
	std::shared_ptr<Material> matRed = std::make_shared<Material>("Solid Red", basicPixelShader, basicVertexShader, XMFLOAT3(0.75f, 0, 0));
	std::shared_ptr<Material> matPurple = std::make_shared<Material>("Solid Purple", basicPixelShader, basicVertexShader, XMFLOAT3(0.75f, 0, 0.6f));

	// Add all materials to vector
	materials.insert(materials.end(), { matFancy, matUV, matNorm, matWhite, matRed, matPurple });

	// Create the game entities
	entities.push_back(std::make_shared<GameEntity>(cubeMesh, matWhite));
	entities.push_back(std::make_shared<GameEntity>(cylinderMesh, matRed));
	entities.push_back(std::make_shared<GameEntity>(helixMesh, matPurple));
	entities.push_back(std::make_shared<GameEntity>(sphereMesh, matFancy));
	entities.push_back(std::make_shared<GameEntity>(torusMesh, matPurple));
	entities.push_back(std::make_shared<GameEntity>(quadMesh, matRed));
	entities.push_back(std::make_shared<GameEntity>(quad2sidedMesh, matWhite));

	// Adjust transforms
	entities[0]->GetTransform()->MoveAbsolute(-9, 0, 0);
	entities[1]->GetTransform()->MoveAbsolute(-6, 0, 0);
	entities[2]->GetTransform()->MoveAbsolute(-3, 0, 0);
	entities[3]->GetTransform()->MoveAbsolute(0, 0, 0);
	entities[4]->GetTransform()->MoveAbsolute(3, 0, 0);
	entities[5]->GetTransform()->MoveAbsolute(6, 0, 0);
	entities[6]->GetTransform()->MoveAbsolute(9, 0, 0);

	// Create more entities using the UV and Normal preview materials
	int count = (int)entities.size();
	for (int i = 0; i < count; i++)
	{
		std::shared_ptr<Mesh> mesh = entities[i]->GetMesh();
		std::shared_ptr<GameEntity> geUV = std::make_shared<GameEntity>(mesh, matUV);
		std::shared_ptr<GameEntity> geNorm = std::make_shared<GameEntity>(mesh, matNorm);

		geUV->GetTransform()->MoveAbsolute(entities[i]->GetTransform()->GetPosition());
		geUV->GetTransform()->MoveAbsolute(0, 3, 0);

		geNorm->GetTransform()->MoveAbsolute(entities[i]->GetTransform()->GetPosition());
		geNorm->GetTransform()->MoveAbsolute(0, 6, 0);

		entities.push_back(geUV);
		entities.push_back(geNorm);
	}
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
	BuildUI(camera, meshes, entities, materials);

	// Example input checking: Quit if the escape key is pressed
	if (Input::KeyDown(VK_ESCAPE))
		Window::Quit();

	// Spin the 3D models
	for (auto& e : entities)
	{
		e->GetTransform()->Rotate(0, deltaTime, 0);
	}

	// Rotate and scale the first one some more
	float scale = (float)sin(totalTime * 5) * 0.5f + 1.0f;
	entities[0]->GetTransform()->SetScale(scale, scale, scale);
	entities[0]->GetTransform()->Rotate(0, 0, deltaTime * 1.0f);

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
		const float color[4] = { 0.4f, 0.6f, 0.75f, 0.0f };
		Graphics::Context->ClearRenderTargetView(Graphics::BackBufferRTV.Get(),	color);
		Graphics::Context->ClearDepthStencilView(Graphics::DepthBufferDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	}

	// DRAW geometry
	// Loop through the game entities and draw each one
	// - Note: A constant buffer has already been bound to
	//   the vertex shader stage of the pipeline (see Init above)
	for (auto& e : entities)
	{
		// Grab the material
		std::shared_ptr<Material> mat = e->GetMaterial();

		// Set up the pipeline for this draw
		Graphics::Context->VSSetShader(mat->GetVertexShader().Get(), 0 ,0);
		Graphics::Context->PSSetShader(mat->GetPixelShader().Get(), 0, 0);

		// Set vertex shader data
		VertexShaderExternalData vsData{};
		vsData.worldMatrix = e->GetTransform()->GetWorldMatrix();
		vsData.viewMatrix = camera->GetView();
		vsData.projectionMatrix = camera->GetProjection();
		Graphics::FillAndBindNextConstantBuffer(&vsData, sizeof(VertexShaderExternalData), D3D11_VERTEX_SHADER, 0);

		// Set pixel shader data
		PixelShaderExternalData psData{};
		psData.colorTint = mat->GetColorTint();
		psData.time = totalTime;
		Graphics::FillAndBindNextConstantBuffer(&psData, sizeof(PixelShaderExternalData), D3D11_PIXEL_SHADER, 0);

		// Draw one entity
		e->Draw();
	}

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
