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
	unsigned int skyboxDescriptorIndex,
	std::shared_ptr<Mesh> mesh)
	:
	skyboxDescriptorIndex(skyboxDescriptorIndex),
	skyMesh(mesh),
	useSphericalHarmonicsForIrradiance(false)
{
	// Init render states and compute IBL resources from environment map
	InitRenderStates();
	CreateIBLResources();
}

// Constructor that loads a DDS cube map file
Sky::Sky(
	const wchar_t* cubemapDDSFile,
	std::shared_ptr<Mesh> mesh,
	bool useSphericalHarmonicsForIrradiance)
	:
	skyMesh(mesh),
	useSphericalHarmonicsForIrradiance(useSphericalHarmonicsForIrradiance)
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
	std::shared_ptr<Mesh> mesh,
	bool useSphericalHarmonicsForIrradiance)
	:
	skyMesh(mesh),
	useSphericalHarmonicsForIrradiance(useSphericalHarmonicsForIrradiance)
{
	// Init render states
	InitRenderStates();

	// Create texture from 6 images
	skyboxDescriptorIndex = Graphics::CreateCubemap(right, left, up, down, front, back);

	// Compute IBL resources from environment map
	CreateIBLResources();
}

Sky::Sky(
	const wchar_t* cubemapDDSFile,
	const wchar_t* irradianceMapDDSFile, 
	const wchar_t* specularMapDDSFile, 
	const wchar_t* brdfLookUpTableDDSFile, 
	unsigned int totalSpecMipLevels,
	std::shared_ptr<Mesh> mesh)
	:
	skyMesh(mesh),
	totalSpecMipLevels(totalSpecMipLevels),
	useSphericalHarmonicsForIrradiance(false)
{
	// Init render states
	InitRenderStates();

	// Load the textures
	skyboxDescriptorIndex = Graphics::LoadTexture(cubemapDDSFile, false);
	irradianceMapDescriptorIndex = Graphics::LoadTexture(irradianceMapDDSFile, false);
	specularMapDescriptorIndex = Graphics::LoadTexture(specularMapDDSFile, false);
	brdfLookUpTableDescriptorIndex = Graphics::LoadTexture(brdfLookUpTableDDSFile, false);
}

Sky::~Sky()
{
}

// Getters
unsigned int Sky::GetSkyboxDescriptorIndex() { return skyboxDescriptorIndex; }
unsigned int Sky::GetBrdfLookUpTableDescriptorIndex() { return brdfLookUpTableDescriptorIndex; }
unsigned int Sky::GetIrradianceMapDescriptorIndex() { return irradianceMapDescriptorIndex; }
unsigned int Sky::GetSpecularMapDescriptorIndex() { return specularMapDescriptorIndex; }
unsigned int Sky::GetTotalSpecularMipLevels() { return totalSpecMipLevels; }

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
	Microsoft::WRL::ComPtr<ID3DBlob> irrMapByteCode;
	Microsoft::WRL::ComPtr<ID3DBlob> specMapByteCode;

	// Load shaders
	{
		D3DReadFileToBlob(FixPath(L"IBLBrdfLookUpTableCS.cso").c_str(), brdfLookUpTableByteCode.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"IBLIrradianceMapCS.cso").c_str(), irrMapByteCode.GetAddressOf());
		D3DReadFileToBlob(FixPath(L"IBLSpecularMapCS.cso").c_str(), specMapByteCode.GetAddressOf());
	}

	// Create compute-specific root sig and PSOs
	{
		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[1] = {};

		// Root params for descriptor indices
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[0].Constants.Num32BitValues = 6; // Max size of all 3 compute shaders
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
		cPSO.pRootSignature = computeRootSig.Get();

		// BRDF look up table
		{
			cPSO.CS.BytecodeLength = brdfLookUpTableByteCode->GetBufferSize();
			cPSO.CS.pShaderBytecode = brdfLookUpTableByteCode->GetBufferPointer();
			Graphics::Device->CreateComputePipelineState(&cPSO, IID_PPV_ARGS(brdfLookUpTablePSO.GetAddressOf()));
		}

		// Irradiance map
		{
			cPSO.CS.BytecodeLength = irrMapByteCode->GetBufferSize();
			cPSO.CS.pShaderBytecode = irrMapByteCode->GetBufferPointer();
			Graphics::Device->CreateComputePipelineState(&cPSO, IID_PPV_ARGS(irradianceMapPSO.GetAddressOf()));
		}

		// Specular map
		{
			cPSO.CS.BytecodeLength = specMapByteCode->GetBufferSize();
			cPSO.CS.pShaderBytecode = specMapByteCode->GetBufferPointer();
			Graphics::Device->CreateComputePipelineState(&cPSO, IID_PPV_ARGS(specularMapPSO.GetAddressOf()));
		}
	}

	// Perform individual compute steps to generate IBL resources
	CreateIBLBrdfLookUpTable();
	CreateIBLSpecularMap();
	CreateIBLIrradianceMap();
}

void Sky::CreateIBLBrdfLookUpTable()
{
	DXGI_FORMAT colorFormat = DXGI_FORMAT_R16G16_UNORM;

	// Create the look up table texture
	brdfLookUpTable = Graphics::CreateTexture(BrdfLookUpTableSize, BrdfLookUpTableSize, 1, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, colorFormat);

	// Create a UAV for it
	D3D12_CPU_DESCRIPTOR_HANDLE uav_cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE uav_gpu;
	Graphics::ReserveDescriptorHeapSlot(&uav_cpu, &uav_gpu);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = colorFormat;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	Graphics::Device->CreateUnorderedAccessView(brdfLookUpTable.Get(), 0, &uavDesc, uav_cpu);

	// Create final SRV for it (using null description to get default SRV)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu;
		D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu;
		Graphics::ReserveDescriptorHeapSlot(&srv_cpu, &srv_gpu);

		Graphics::Device->CreateShaderResourceView(brdfLookUpTable.Get(), 0, srv_cpu);
		brdfLookUpTableDescriptorIndex = Graphics::GetDescriptorIndex(srv_gpu);
	}

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
	// Calculate how many mip levels we'll need, potentially skipping
	// a few of the smaller levels (1x1, 2x2, etc.) because they're mostly
	// the same with such low resolutions
	totalSpecMipLevels = max((int)(log2(SpecularMapSize)) + 1 - SpecMipLevelsToSkip, 1); // Add 1 for 1x1

	// Create the irradiance map as a 6-element texture array
	// (a cube map!) with multiple mip levels
	specularMap = Graphics::CreateTexture(SpecularMapSize, SpecularMapSize, 6, totalSpecMipLevels, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	// Create final SRV for it
	{
		D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu;
		D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu;
		Graphics::ReserveDescriptorHeapSlot(&srv_cpu, &srv_gpu);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = totalSpecMipLevels;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Graphics::Device->CreateShaderResourceView(specularMap.Get(), &srvDesc, srv_cpu);
		specularMapDescriptorIndex = Graphics::GetDescriptorIndex(srv_gpu);
	}

	// Run the compute shader to create the specular map,
	// one mip level at a time since you cannot access
	// individual mips of a RWTexture in a compute shader :/
	for (unsigned int mip = 0; mip < totalSpecMipLevels; mip++)
	{
		// Create a UAV for this mip level
		D3D12_CPU_DESCRIPTOR_HANDLE uav_cpu;
		D3D12_GPU_DESCRIPTOR_HANDLE uav_gpu;
		Graphics::ReserveDescriptorHeapSlot(&uav_cpu, &uav_gpu);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		uavDesc.Texture2DArray.ArraySize = 6;
		uavDesc.Texture2DArray.FirstArraySlice = 0;
		uavDesc.Texture2DArray.MipSlice = mip;
		Graphics::Device->CreateUnorderedAccessView(specularMap.Get(), 0, &uavDesc, uav_cpu);

		// Set up compute pipeline
		Graphics::CommandList->SetDescriptorHeaps(1, Graphics::CBVSRVDescriptorHeap.GetAddressOf());
		Graphics::CommandList->SetComputeRootSignature(computeRootSig.Get());

		// Calculate the size of this mip level
		unsigned int mipSize = (unsigned int)pow(2, totalSpecMipLevels + SpecMipLevelsToSkip - 1 - mip);

		SpecularComputeIndices data{};
		data.EnvironmentMapDescriptorIndex = skyboxDescriptorIndex;
		data.OutputDescriptorIndex = Graphics::GetDescriptorIndex(uav_gpu);
		data.OutputWidth = mipSize;
		data.OutputHeight = mipSize;
		data.MipLevel = mip;
		data.Roughness = mip / (float)(totalSpecMipLevels - 1);
		Graphics::CommandList->SetComputeRoot32BitConstants(
			0,
			sizeof(SpecularComputeIndices) / sizeof(unsigned int),
			&data,
			0);

		Graphics::CommandList->SetPipelineState(specularMapPSO.Get());
		Graphics::CommandList->Dispatch(SpecularMapSize / 8, SpecularMapSize / 8, 6); // 6 on Z for cube map!
	}
}

void Sky::CreateIBLIrradianceMap()
{
	// Create the irradiance map as a 6-element texture array (a cube map!)
	irradianceMap = Graphics::CreateTexture(IrradianceMapSize, IrradianceMapSize, 6, 1, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	// Create a UAV for it
	D3D12_CPU_DESCRIPTOR_HANDLE uav_cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE uav_gpu;
	Graphics::ReserveDescriptorHeapSlot(&uav_cpu, &uav_gpu);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
	uavDesc.Texture2DArray.ArraySize = 6;
	uavDesc.Texture2DArray.FirstArraySlice = 0;
	uavDesc.Texture2DArray.MipSlice = 0;
	Graphics::Device->CreateUnorderedAccessView(irradianceMap.Get(), 0, &uavDesc, uav_cpu);

	// Create final SRV for it
	{
		D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu;
		D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu;
		Graphics::ReserveDescriptorHeapSlot(&srv_cpu, &srv_gpu);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = uavDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		Graphics::Device->CreateShaderResourceView(irradianceMap.Get(), &srvDesc, srv_cpu);
		irradianceMapDescriptorIndex = Graphics::GetDescriptorIndex(srv_gpu);
	}

	// Run the compute shader to create the irradiance map
	{
		Graphics::CommandList->SetDescriptorHeaps(1, Graphics::CBVSRVDescriptorHeap.GetAddressOf());
		Graphics::CommandList->SetComputeRootSignature(computeRootSig.Get());

		IrradianceComputeIndices data{};
		data.EnvironmentMapDescriptorIndex = skyboxDescriptorIndex;
		data.OutputDescriptorIndex = Graphics::GetDescriptorIndex(uav_gpu);
		data.OutputWidth = IrradianceMapSize;
		data.OutputHeight = IrradianceMapSize;
		Graphics::CommandList->SetComputeRoot32BitConstants(
			0,
			sizeof(IrradianceComputeIndices) / sizeof(unsigned int),
			&data,
			0);

		Graphics::CommandList->SetPipelineState(irradianceMapPSO.Get());
		Graphics::CommandList->Dispatch(IrradianceMapSize / 8, IrradianceMapSize / 8, 6); // 6 on Z for cube map!
	}

	// TEST
	std::vector<unsigned char> pixels;
	Graphics::ReadTextureDataFromGPU(irradianceMap, pixels);

}

void Sky::CreateIBLIrradianceSphericalHarmonics(Microsoft::WRL::ComPtr<ID3D12Resource> skyCube)
{
	std::vector<unsigned char> pixelData;
	Graphics::ReadTextureDataFromGPU(skyCube, pixelData);
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
