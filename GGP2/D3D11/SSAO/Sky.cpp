#include "Sky.h"
#include "Graphics.h"
#include "WICTextureLoader.h"
#include "DDSTextureLoader.h"

using namespace DirectX;

// Constructor that takes an existing cube map SRV
Sky::Sky(
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubeMap,
	std::shared_ptr<Mesh> mesh,
	std::shared_ptr<SimpleVertexShader> skyVS,
	std::shared_ptr<SimplePixelShader> skyPS,
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
	IBLOptions& iblOptions)
	:
	skySRV(cubeMap),
	skyMesh(mesh),
	samplerOptions(samplerOptions),
	skyVS(skyVS),
	skyPS(skyPS)
{
	// Init render states
	InitRenderStates();

	// Build IBL Maps
	IBLCreateIrradianceMap(iblOptions);
	IBLCreateConvolvedSpecularMap(iblOptions);
	IBLCreateBRDFLookUpTexture(iblOptions);
}

// Constructor that loads a DDS cube map file
Sky::Sky(
	const wchar_t* cubemapDDSFile, 
	std::shared_ptr<Mesh> mesh,
	std::shared_ptr<SimpleVertexShader> skyVS,
	std::shared_ptr<SimplePixelShader> skyPS,
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
	IBLOptions& iblOptions)
	:
	skyMesh(mesh),
	samplerOptions(samplerOptions),
	skyVS(skyVS),
	skyPS(skyPS)
{
	// Init render states
	InitRenderStates();

	// Load texture
	CreateDDSTextureFromFile(Graphics::Device.Get(), cubemapDDSFile, 0, skySRV.GetAddressOf());

	// Build IBL Maps
	IBLCreateIrradianceMap(iblOptions);
	IBLCreateConvolvedSpecularMap(iblOptions);
	IBLCreateBRDFLookUpTexture(iblOptions);
}

// Constructor that loads 6 textures and makes a cube map
Sky::Sky(
	const wchar_t* right, 
	const wchar_t* left, 
	const wchar_t* up, 
	const wchar_t* down, 
	const wchar_t* front, 
	const wchar_t* back, 
	std::shared_ptr<Mesh> mesh,
	std::shared_ptr<SimpleVertexShader> skyVS,
	std::shared_ptr<SimplePixelShader> skyPS,
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
	IBLOptions& iblOptions)
	:
	skyMesh(mesh),
	samplerOptions(samplerOptions),
	skyVS(skyVS),
	skyPS(skyPS)
{
	// Init render states
	InitRenderStates();

	// Create texture from 6 images
	skySRV = CreateCubemap(right, left, up, down, front, back);

	// Build IBL Maps
	IBLCreateIrradianceMap(iblOptions);
	IBLCreateConvolvedSpecularMap(iblOptions);
	IBLCreateBRDFLookUpTexture(iblOptions);
}

Sky::~Sky()
{
}

void Sky::Draw(std::shared_ptr<Camera> camera)
{
	// Change to the sky-specific rasterizer state
	Graphics::Context->RSSetState(skyRasterState.Get());
	Graphics::Context->OMSetDepthStencilState(skyDepthState.Get(), 0);

	// Set the sky shaders
	skyVS->SetShader();
	skyPS->SetShader();

	// Give them proper data
	skyVS->SetMatrix4x4("view", camera->GetView());
	skyVS->SetMatrix4x4("projection", camera->GetProjection());
	skyVS->CopyAllBufferData();

	// Send the proper resources to the pixel shader
	skyPS->SetShaderResourceView("SkyTexture", skySRV);
	skyPS->SetSamplerState("BasicSampler", samplerOptions);

	// Set mesh buffers and draw
	skyMesh->SetBuffersAndDraw();

	// Reset my rasterizer state to the default
	Graphics::Context->RSSetState(0); // Null (or 0) puts back the defaults
	Graphics::Context->OMSetDepthStencilState(0, 0);
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::GetSkyTexture() { return skySRV; }
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::GetIrradianceIBLMap() { return irradianceIBL; }
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::GetSpecularIBLMap() { return specularIBL; }
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::GetBRDFLookUpTexture() { return brdfLookUpMap; }
int Sky::GetTotalSpecularIBLMipLevels() { return totalSpecIBLMipLevels; }

void Sky::InitRenderStates()
{
	// Rasterizer to reverse the cull mode
	D3D11_RASTERIZER_DESC rastDesc = {};
	rastDesc.CullMode = D3D11_CULL_FRONT; // Draw the inside instead of the outside!
	rastDesc.FillMode = D3D11_FILL_SOLID;
	rastDesc.DepthClipEnable = true;
	Graphics::Device->CreateRasterizerState(&rastDesc, skyRasterState.GetAddressOf());

	// Depth state so that we ACCEPT pixels with a depth == 1
	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = true;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	Graphics::Device->CreateDepthStencilState(&depthDesc, skyDepthState.GetAddressOf());
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Sky::CreateCubemap(const wchar_t* right, const wchar_t* left, const wchar_t* up, const wchar_t* down, const wchar_t* front, const wchar_t* back)
{
	// Load the 6 textures into an array.
	// - We need references to the TEXTURES, not SHADER RESOURCE VIEWS!
	// - Explicitly NOT generating mipmaps, as we don't need them for the sky!
	// - Order matters here!  +X, -X, +Y, -Y, +Z, -Z
	Microsoft::WRL::ComPtr<ID3D11Texture2D> textures[6] = {};
	CreateWICTextureFromFile(Graphics::Device.Get(), right, (ID3D11Resource**)textures[0].GetAddressOf(), 0);
	CreateWICTextureFromFile(Graphics::Device.Get(), left, (ID3D11Resource**)textures[1].GetAddressOf(), 0);
	CreateWICTextureFromFile(Graphics::Device.Get(), up, (ID3D11Resource**)textures[2].GetAddressOf(), 0);
	CreateWICTextureFromFile(Graphics::Device.Get(), down, (ID3D11Resource**)textures[3].GetAddressOf(), 0);
	CreateWICTextureFromFile(Graphics::Device.Get(), front, (ID3D11Resource**)textures[4].GetAddressOf(), 0);
	CreateWICTextureFromFile(Graphics::Device.Get(), back, (ID3D11Resource**)textures[5].GetAddressOf(), 0);

	// We'll assume all of the textures are the same color format and resolution,
	// so get the description of the first shader resource view
	D3D11_TEXTURE2D_DESC faceDesc = {};
	textures[0]->GetDesc(&faceDesc);

	// Describe the resource for the cube map, which is simply 
	// a "texture 2d array" with the TEXTURECUBE flag set.  
	// This is a special GPU resource format, NOT just a 
	// C++ array of textures!!!
	D3D11_TEXTURE2D_DESC cubeDesc = {};
	cubeDesc.ArraySize = 6; // Cube map!
	cubeDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // We'll be using as a texture in a shader
	cubeDesc.CPUAccessFlags = 0; // No read back
	cubeDesc.Format = faceDesc.Format; // Match the loaded texture's color format
	cubeDesc.Width = faceDesc.Width;  // Match the size
	cubeDesc.Height = faceDesc.Height; // Match the size
	cubeDesc.MipLevels = 1; // Only need 1
	cubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // This should be treated as a CUBE, not 6 separate textures
	cubeDesc.Usage = D3D11_USAGE_DEFAULT; // Standard usage
	cubeDesc.SampleDesc.Count = 1;
	cubeDesc.SampleDesc.Quality = 0;

	// Create the final texture resource to hold the cube map
	Microsoft::WRL::ComPtr<ID3D11Texture2D> cubeMapTexture;
	Graphics::Device->CreateTexture2D(&cubeDesc, 0, cubeMapTexture.GetAddressOf());

	// Loop through the individual face textures and copy them,
	// one at a time, to the cube map texure
	for (int i = 0; i < 6; i++)
	{
		// Calculate the subresource position to copy into
		unsigned int subresource = D3D11CalcSubresource(
			0,	// Which mip (zero, since there's only one)
			i,	// Which array element?
			1); // How many mip levels are in the texture?

		// Copy from one resource (texture) to another
		Graphics::Context->CopySubresourceRegion(
			cubeMapTexture.Get(),	// Destination resource
			subresource,			// Dest subresource index (one of the array elements)
			0, 0, 0,				// XYZ location of copy
			textures[i].Get(),		// Source resource
			0,						// Source subresource index (we're assuming there's only one)
			0);						// Source subresource "box" of data to copy (zero means the whole thing)
	}

	// At this point, all of the faces have been copied into the 
	// cube map texture, so we can describe a shader resource view for it
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = cubeDesc.Format; // Same format as texture
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE; // Treat this as a cube!
	srvDesc.TextureCube.MipLevels = 1;	// Only need access to 1 mip
	srvDesc.TextureCube.MostDetailedMip = 0; // Index of the first mip we want to see

	// Make the SRV
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubeSRV;
	Graphics::Device->CreateShaderResourceView(cubeMapTexture.Get(), &srvDesc, cubeSRV.GetAddressOf());

	// Send back the SRV, which is what we need for our shaders
	return cubeSRV;
}

// ----------------------------------------------------------------------------
// Given the cube environment map of this environment, compute the irradiance
// cube map for indirect diffuse lighting.  This requires rendering each face
// of the resulting cube map, one at a time.
// ----------------------------------------------------------------------------
void Sky::IBLCreateIrradianceMap(IBLOptions& iblOptions)
{
	printf("Creating IBL irradiance map for indirect diffuse lighting...");

	// == The D3D resources we'll need ===============================

	Microsoft::WRL::ComPtr<ID3D11Texture2D> irrMapFinalTexture;	// Final texture once we have the full irradiance


	// == Set up D3D resources =======================================

	// Create the final irradiance cube texture
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = IBLCubeSize;
	texDesc.Height = IBLCubeSize;
	texDesc.ArraySize = 6; // Cube map means 6 textures
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // Basic texture format
	texDesc.MipLevels = 1; // No mip chain needed
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // It's a cube map
	texDesc.SampleDesc.Count = 1; // Can't be zero
	Graphics::Device->CreateTexture2D(&texDesc, 0, irrMapFinalTexture.GetAddressOf());

	// Create an SRV for the irradiance texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;	// Treat this resource as a cube map
	srvDesc.TextureCube.MipLevels = 1;			// Only 1 mip level
	srvDesc.TextureCube.MostDetailedMip = 0;	// Accessing the first (and only) mip
	srvDesc.Format = texDesc.Format;			// Same format as texture
	Graphics::Device->CreateShaderResourceView(irrMapFinalTexture.Get(), &srvDesc, irradianceIBL.GetAddressOf());



	// == Save previous states ===================================

	// Save current render target and depth buffer
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> prevRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> prevDSV;
	Graphics::Context->OMGetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.GetAddressOf());

	// Save current viewport
	unsigned int vpCount = 1;
	D3D11_VIEWPORT prevVP = {};
	Graphics::Context->RSGetViewports(&vpCount, &prevVP);



	// == Set current states for rendering =======================

	// Make sure the viewport matches the texture size
	D3D11_VIEWPORT vp = {};
	vp.Width = (float)IBLCubeSize;
	vp.Height = (float)IBLCubeSize;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	Graphics::Context->RSSetViewports(1, &vp);

	// Set states that may or may not be set yet
	Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



	// == Set up shaders ============================================

	iblOptions.FullscreenVS->SetShader();
	iblOptions.IBLIrradiancePS->SetShader();
	iblOptions.IBLIrradiancePS->SetShaderResourceView("EnvironmentMap", skySRV.Get());
	iblOptions.IBLIrradiancePS->SetSamplerState("BasicSampler", samplerOptions.Get());


	// == Render irradiance =======================================

	// Loop through the six cubemap faces and calculate 
	// the irradiance for each, resulting in a full 360 degree
	// irradiance map, once completed
	for (int face = 0; face < 6; face++)
	{
		// Make a render target view for this face
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;	// This points to a Texture2D Array
		rtvDesc.Texture2DArray.ArraySize = 1;			// How much of the array do we need access to?
		rtvDesc.Texture2DArray.FirstArraySlice = face;	// Which texture are we rendering into?
		rtvDesc.Texture2DArray.MipSlice = 0;			// Which mip of that texture are we rendering into?
		rtvDesc.Format = texDesc.Format;				// Same format as texture

		// Create the render target view itself
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
		Graphics::Device->CreateRenderTargetView(irrMapFinalTexture.Get(), &rtvDesc, rtv.GetAddressOf());

		// Clear and set this render target
		float black[4] = {}; // Initialize to all zeroes
		Graphics::Context->ClearRenderTargetView(rtv.Get(), black);
		Graphics::Context->OMSetRenderTargets(1, rtv.GetAddressOf(), 0);

		// Per-face shader data and copy
		iblOptions.IBLIrradiancePS->SetInt("faceIndex", face);
		iblOptions.IBLIrradiancePS->SetFloat("sampleStepPhi", 0.025f);
		iblOptions.IBLIrradiancePS->SetFloat("sampleStepTheta", 0.025f);
		iblOptions.IBLIrradiancePS->CopyAllBufferData();

		// Render exactly 3 vertices
		Graphics::Context->Draw(3, 0);

		// Ensure we flush the graphics pipe so we don't cause 
		// a hardware timeout which can result in a driver crash
		// NOTE: This might make C++ sit and wait for a sec!  Better than a crash!
		Graphics::Context->Flush();
	}


	// == Restore old states =========================

	// Restore the old render target and viewport
	Graphics::Context->OMSetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.Get());
	Graphics::Context->RSSetViewports(1, &prevVP);

	printf("done!\n");
}



// ----------------------------------------------------------------------------
// Given the cube environment map of this environment, compute the convolved (blurred)
// cube map for indirect specular lighting - basically the blurry reflections based
// on the roughness of the surface.  This requires not only rendering each face
// of the resulting cube map, one at a time, but doing so for each mip map level
// of the resulting cube map, as more "blurry" reflections are stored in successive
// mip levels of the convolved (blurred) map.
// ----------------------------------------------------------------------------
void Sky::IBLCreateConvolvedSpecularMap(IBLOptions& iblOptions)
{
	printf("Creating convolved environment map for indirect specular lighting...");

	// Calculate how many mip levels we'll need, potentially skipping
	// a few of the smaller levels (1x1, 2x2, etc.) because they're mostly
	// the same with such low resolutions
	totalSpecIBLMipLevels = max((int)(log2(IBLCubeSize)) + 1 - specIBLMipLevelsToSkip, 1); // Add 1 for 1x1

	// == The resources we'll need ===============================

	Microsoft::WRL::ComPtr<ID3D11Texture2D> specConvFinalTexture;	// Final texture once we have the full specular ibl


	// == Set up resources =======================================

	// Create the final conv cube texture
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = IBLCubeSize;
	texDesc.Height = IBLCubeSize;
	texDesc.ArraySize = 6; // Cube map means 6 textures
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // Basic texture format
	texDesc.MipLevels = totalSpecIBLMipLevels; // Depends on face size
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // It's a cube map
	texDesc.SampleDesc.Count = 1; // Can't be zero
	Graphics::Device->CreateTexture2D(&texDesc, 0, specConvFinalTexture.GetAddressOf());

	// Create an SRV for the texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;	// Treat this resource as a cube map
	srvDesc.TextureCube.MipLevels = totalSpecIBLMipLevels;			// Depends on face size
	srvDesc.TextureCube.MostDetailedMip = 0;	// Accessing the first (and only) mip
	srvDesc.Format = texDesc.Format;			// Same format as texture
	Graphics::Device->CreateShaderResourceView(specConvFinalTexture.Get(), &srvDesc, specularIBL.GetAddressOf());


	// == Save previous DX states ===================================

	// Save current render target and depth buffer
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> prevRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> prevDSV;
	Graphics::Context->OMGetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.GetAddressOf());

	// Save current viewport
	unsigned int vpCount = 1;
	D3D11_VIEWPORT prevVP = {};
	Graphics::Context->RSGetViewports(&vpCount, &prevVP);




	// == Set current states for rendering =======================

	// Set states that may or may not be set yet
	Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



	// == Set up shaders ============================================
	iblOptions.FullscreenVS->SetShader();
	iblOptions.IBLSpecularConvolutionPS->SetShader();
	iblOptions.IBLSpecularConvolutionPS->SetShaderResourceView("EnvironmentMap", skySRV.Get());
	iblOptions.IBLSpecularConvolutionPS->SetSamplerState("BasicSampler", samplerOptions.Get());


	// == Render convolution =======================================

	for (int mip = 0; mip < totalSpecIBLMipLevels; mip++)
	{

		// Loop through the six cubemap faces and calculate 
		// the convultion for each, resulting in a full 360 degree
		// irradiance map, once completed
		for (int face = 0; face < 6; face++)
		{
			// Make a render target view for this face
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;	// This points to a Texture2D Array
			rtvDesc.Texture2DArray.ArraySize = 1;			// How much of the array do we have access to?
			rtvDesc.Texture2DArray.FirstArraySlice = face;	// Which texture are we rendering into?
			rtvDesc.Texture2DArray.MipSlice = mip;			// Which mip of that texture are we rendering into?
			rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;	// Same format as accum texture

			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
			Graphics::Device->CreateRenderTargetView(specConvFinalTexture.Get(), &rtvDesc, rtv.GetAddressOf());

			// Clear and set this render target
			float black[4] = {}; // Initialize to all zeroes
			Graphics::Context->ClearRenderTargetView(rtv.Get(), black);
			Graphics::Context->OMSetRenderTargets(1, rtv.GetAddressOf(), 0);

			// Create a viewport that matches the size of this MIP
			D3D11_VIEWPORT vp = {};
			vp.Width = (float)pow(2, totalSpecIBLMipLevels + specIBLMipLevelsToSkip - 1 - mip);
			vp.Height = vp.Width; // Always square
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			Graphics::Context->RSSetViewports(1, &vp);

			// Handle per-face shader data and copy
			iblOptions.IBLSpecularConvolutionPS->SetFloat("roughness", mip / (float)(totalSpecIBLMipLevels - 1));
			iblOptions.IBLSpecularConvolutionPS->SetInt("faceIndex", face);
			iblOptions.IBLSpecularConvolutionPS->SetInt("mipLevel", mip);
			iblOptions.IBLSpecularConvolutionPS->CopyAllBufferData();

			// Render exactly 3 vertices
			Graphics::Context->Draw(3, 0);

			// Ensure we flush the graphics pipe to 
			// so that we don't cause a hardware timeout
			// which can result in a driver crash
			// NOTE: This might make C++ sit and wait!  Better than a crash!
			Graphics::Context->Flush();
		}

	}


	// == Restore old states =========================

	// Restore the old render target and release the resources, as 
	// they each got an extra ref when we used OMGetRenderTargets() above
	Graphics::Context->OMSetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.Get());
	Graphics::Context->RSSetViewports(1, &prevVP);

	printf("done!\n");
}



// ----------------------------------------------------------------------------
// Generates a texture containing pre-computed values used during indirect
// specular lighting (environment reflections).  This texture is always the 
// same, regardless of the environment.  This is pre-computed, because each
// pixel needs to do thousands of computations to get the "correct" value, 
// but is always the same "correct" value.
//
// It could technically be generated once, saved out and loaded back in, 
// rather than re-computing it each time.
// ----------------------------------------------------------------------------
void Sky::IBLCreateBRDFLookUpTexture(IBLOptions& iblOptions)
{
	printf("Creating pre-calculated environment BRDF lookup texture...");

	// == The DX resources we'll need ===============================
	Microsoft::WRL::ComPtr<ID3D11Texture2D> envBrdfFinalTexture;	// Final texture once we have the full specular ibl


	// == Set up resources =======================================

	// Create the final look up texture
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = IBLLookUpTextureSize;
	texDesc.Height = IBLLookUpTextureSize;
	texDesc.ArraySize = 1; // Single texture
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.Format = DXGI_FORMAT_R16G16_FLOAT;  // Only two channels, each of which is double the precision
	texDesc.MipLevels = 1; // Just one mip level
	texDesc.MiscFlags = 0; // NOT a cube map!
	texDesc.SampleDesc.Count = 1; // Can't be zero
	Graphics::Device->CreateTexture2D(&texDesc, 0, envBrdfFinalTexture.GetAddressOf());

	// Create an SRV for the BRDF look-up texture
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;	// Just a regular 2d texture
	srvDesc.Texture2D.MipLevels = 1;			// Just one
	srvDesc.Texture2D.MostDetailedMip = 0;		// Accessing the first (and only) mip
	srvDesc.Format = texDesc.Format;			// Same format as texture
	Graphics::Device->CreateShaderResourceView(envBrdfFinalTexture.Get(), &srvDesc, brdfLookUpMap.GetAddressOf());


	// == Save previous states ===================================

	// Save current render target and depth buffer
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> prevRTV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> prevDSV;
	Graphics::Context->OMGetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.GetAddressOf());

	// Save current viewport
	unsigned int vpCount = 1;
	D3D11_VIEWPORT prevVP = {};
	Graphics::Context->RSGetViewports(&vpCount, &prevVP);



	// == Set current states for rendering =======================

	// Create a viewport that matches the size of this texture
	D3D11_VIEWPORT vp = {};
	vp.Width = (float)IBLLookUpTextureSize;
	vp.Height = (float)IBLLookUpTextureSize;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	Graphics::Context->RSSetViewports(1, &vp);

	// Set states that may or may not be set yet
	Graphics::Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



	// == Set up shaders ============================================
	iblOptions.FullscreenVS->SetShader();
	iblOptions.IBLBRDFLookUpPS->SetShader();


	// == Render look up table =======================================


	// Make a render target view for this whole texture
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;	// This points to a Texture2D
	rtvDesc.Texture2D.MipSlice = 0;							// Which mip of that texture are we rendering into?
	rtvDesc.Format = texDesc.Format;						// Match the format of the texture

	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
	Graphics::Device->CreateRenderTargetView(envBrdfFinalTexture.Get(), &rtvDesc, rtv.GetAddressOf());

	// Clear and set this render target
	float black[4] = {}; // Initialize to all zeroes
	Graphics::Context->ClearRenderTargetView(rtv.Get(), black);
	Graphics::Context->OMSetRenderTargets(1, rtv.GetAddressOf(), 0);

	// Render exactly 3 vertices
	Graphics::Context->Draw(3, 0);

	// Ensure we flush the graphics pipe
	// so that we don't cause a hardware timeout
	// which can result in a driver crash
	// NOTE: This might make C++ sit and wait!  Better than a crash!
	Graphics::Context->Flush();


	// == Restore old states =========================

	// Restore the old render target and viewport
	Graphics::Context->OMSetRenderTargets(1, prevRTV.GetAddressOf(), prevDSV.Get());
	Graphics::Context->RSSetViewports(1, &prevVP);


	// Save SRV for debug
	iblOptions.BRDFLookUpSRV = brdfLookUpMap;

	printf("done!\n");
}
