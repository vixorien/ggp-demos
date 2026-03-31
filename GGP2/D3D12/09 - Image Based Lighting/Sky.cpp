#include "Sky.h"
#include "Graphics.h"
#include "WICTextureLoader.h"
#include "DDSTextureLoader.h"
#include "BufferStructs.h"
#include "PathHelpers.h"

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

using namespace DirectX;

// Constructor that takes an existing cube map SRV index
Sky::Sky(
	std::shared_ptr<Mesh> mesh,
	unsigned int skyboxDescriptorIndex) 
	:
	skyboxDescriptorIndex(skyboxDescriptorIndex),
	skyMesh(mesh)
{
	// Init render states and compute IBL resources from environment map
	InitRenderStates();
	CreateIBLResources();
}

// Constructor that loads a DDS cube map file
Sky::Sky(
	const wchar_t* cubemapDDSFile,
	std::shared_ptr<Mesh> mesh) 
	:
	skyMesh(mesh)
{
	// Init render states
	InitRenderStates();

	// Load the texture
	skyboxDescriptorIndex = Graphics::LoadTexture(cubemapDDSFile, false);

	// Compute IBL resources from environment map
	CreateIBLResources();
}

// Constructor that loads 6 textures and makes a cube map
Sky::Sky(
	const wchar_t* right, 
	const wchar_t* left, 
	const wchar_t* up, 
	const wchar_t* down, 
	const wchar_t* front, 
	const wchar_t* back, 
	std::shared_ptr<Mesh> mesh)
	:
	skyMesh(mesh)
{
	// Init render states
	InitRenderStates();

	// Create texture from 6 images
	skyboxDescriptorIndex = Graphics::CreateCubemap(right, left, up, down, front, back);

	// Compute IBL resources from environment map
	CreateIBLResources();
}

Sky::~Sky()
{
}

// Getters
unsigned int Sky::GetSkyboxDescriptorIndex() { return skyboxDescriptorIndex; }
unsigned int Sky::GetBrdfLookUpTableDescriptorIndex() { return brdfLookUpTableDescriptorIndex; }

void Sky::InitRenderStates()
{
	// Root Signature
	{
		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[1] = {};

		// Root params for descriptor indices
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[0].Constants.Num32BitValues = sizeof(SkyDrawIndices) / sizeof(unsigned int);
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
		// Load sky-specific shaders
		Microsoft::WRL::ComPtr<ID3DBlob> vsByteCode;
		Microsoft::WRL::ComPtr<ID3DBlob> psByteCode;
		D3DReadFileToBlob(FixPath(L"SkyVS.cso").c_str(), vsByteCode.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"SkyPS.cso").c_str(), psByteCode.GetAddressOf());

		// Describe the pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

		// -- Input assembler related ---
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// Root sig
		psoDesc.pRootSignature = rootSignature.Get();

		// -- Shaders (VS/PS) --- 
		psoDesc.VS.pShaderBytecode = vsByteCode->GetBufferPointer();
		psoDesc.VS.BytecodeLength = vsByteCode->GetBufferSize();
		psoDesc.PS.pShaderBytecode = psByteCode->GetBufferPointer();
		psoDesc.PS.BytecodeLength = psByteCode->GetBufferSize();

		// -- Render targets ---
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		// -- States ---
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; // Inside of sky!
		psoDesc.RasterizerState.DepthClipEnable = true;

		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // Accept depth == 1
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
}

void Sky::CreateIBLResources()
{
	// Shaders for PSOs
	Microsoft::WRL::ComPtr<ID3DBlob> brdfLookUpTableByteCode;

	// Load shaders
	{
		D3DReadFileToBlob(FixPath(L"IBLBrdfLookUpTableCS.cso").c_str(), brdfLookUpTableByteCode.GetAddressOf());
	}

	// Create compute-specific root sig and PSOs
	{
		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[1] = {};

		// Root params for descriptor indices
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[0].Constants.Num32BitValues = 4; // TODO: UPDATE
		rootParams[0].Constants.RegisterSpace = 0;
		rootParams[0].Constants.ShaderRegister = 0;

		D3D12_STATIC_SAMPLER_DESC anisoWrap = {};
		anisoWrap.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		anisoWrap.Filter = D3D12_FILTER_ANISOTROPIC;
		anisoWrap.MaxAnisotropy = 16;
		anisoWrap.MaxLOD = D3D12_FLOAT32_MAX;
		anisoWrap.ShaderRegister = 0;  // register(s0)
		anisoWrap.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

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
			IID_PPV_ARGS(computeRootSig.GetAddressOf()));

		// Set up PSOs
		D3D12_COMPUTE_PIPELINE_STATE_DESC cPSO{};
		cPSO.CS.BytecodeLength = brdfLookUpTableByteCode->GetBufferSize();
		cPSO.CS.pShaderBytecode = brdfLookUpTableByteCode->GetBufferPointer();
		cPSO.pRootSignature = computeRootSig.Get();

		Graphics::Device->CreateComputePipelineState(&cPSO, IID_PPV_ARGS(brdfLookUpTablePSO.GetAddressOf()));
	}

	// Perform individual compute steps to generate IBL resources
	CreateIBLBrdfLookUpTable();
	CreateIBLSpecularMap();
	CreateIBLIrradianceMap();
}

void Sky::CreateIBLBrdfLookUpTable()
{
	// Create the look up table texture
	brdfLookUpTable = Graphics::CreateTexture(BrdfLookUpTableSize, BrdfLookUpTableSize, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	// Create a UAV for it
	D3D12_CPU_DESCRIPTOR_HANDLE uav_cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE uav_gpu;
	Graphics::ReserveDescriptorHeapSlot(&uav_cpu, &uav_gpu);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	Graphics::Device->CreateUnorderedAccessView(brdfLookUpTable.Get(), 0, &uavDesc, uav_cpu);

	// Create final SRV for it
	D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu;
	Graphics::ReserveDescriptorHeapSlot(&srv_cpu, &srv_gpu);

	// Using a null description to make a default SRV
	Graphics::Device->CreateShaderResourceView(brdfLookUpTable.Get(), 0, srv_cpu);
	brdfLookUpTableDescriptorIndex = Graphics::GetDescriptorIndex(srv_gpu);

	// Run the compute shader to create the brdf look up table
	{
		Graphics::CommandList->SetDescriptorHeaps(1, Graphics::CBVSRVDescriptorHeap.GetAddressOf());
		Graphics::CommandList->SetComputeRootSignature(computeRootSig.Get());

		BrdfLUTComputeIndices data{};
		data.OutputDescriptorIndex = Graphics::GetDescriptorIndex(uav_gpu);
		data.OutputWidth = BrdfLookUpTableSize;
		data.OutputHeight = BrdfLookUpTableSize;
		Graphics::CommandList->SetComputeRoot32BitConstants(
			0,
			sizeof(BrdfLUTComputeIndices) / sizeof(unsigned int),
			&data,
			0);

		Graphics::CommandList->SetPipelineState(brdfLookUpTablePSO.Get());
		Graphics::CommandList->Dispatch(BrdfLookUpTableSize / 8, BrdfLookUpTableSize / 8, 1);
	}
}

void Sky::CreateIBLSpecularMap()
{
}

void Sky::CreateIBLIrradianceMap()
{
}

void Sky::Draw(std::shared_ptr<Camera> camera)
{
	// Set pipeline stuff (assuming we're using the heap from Game)
	Graphics::CommandList->SetPipelineState(pipelineState.Get());
	Graphics::CommandList->SetGraphicsRootSignature(rootSignature.Get());

	// Basic draw data
	SkyDrawIndices drawData{};
	drawData.psSkyboxIndex = skyboxDescriptorIndex;
	drawData.vsVertexBufferIndex = Graphics::GetDescriptorIndex(skyMesh->GetVertexBufferDescriptorHandle());
	
	// Per frame data
	{
		VertexShaderPerFrameData vsFrame{};
		vsFrame.view = camera->GetView();
		vsFrame.projection = camera->GetProjection();

		D3D12_GPU_DESCRIPTOR_HANDLE cbHandleVS = Graphics::FillNextConstantBufferAndGetGPUDescriptorHandle(
			(void*)(&vsFrame), sizeof(VertexShaderPerFrameData));

		drawData.vsCBIndex = Graphics::GetDescriptorIndex(cbHandleVS);
	}

	// Copy draw constants
	Graphics::CommandList->SetGraphicsRoot32BitConstants(
		0,
		sizeof(SkyDrawIndices) / sizeof(unsigned int),
		&drawData,
		0);

	// Grab the mesh and its buffer views
	D3D12_INDEX_BUFFER_VIEW ibv = skyMesh->GetIndexBufferView();

	// Set the geometry
	Graphics::CommandList->IASetIndexBuffer(&ibv);

	// Draw
	Graphics::CommandList->DrawIndexedInstanced((UINT)skyMesh->GetIndexCount(), 1, 0, 0, 0);
}
