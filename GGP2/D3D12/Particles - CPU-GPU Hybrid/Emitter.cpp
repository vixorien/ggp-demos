#include "Emitter.h"
#include "Graphics.h"
#include "PathHelpers.h"

// Needed for a helper function to load pre-compiled shader files
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>

// Helper macro for getting a float between min and max
#define RandomRange(min, max) ((float)rand() / RAND_MAX * (max - min) + min)

using namespace DirectX;

Emitter::Emitter(
	int maxParticles,
	int particlesPerSecond,
	float lifetime,
	float startSize,
	float endSize,
	bool constrainYAxis,
	DirectX::XMFLOAT4 startColor,
	DirectX::XMFLOAT4 endColor,
	DirectX::XMFLOAT3 startVelocity,
	DirectX::XMFLOAT3 velocityRandomRange,
	DirectX::XMFLOAT3 emitterPosition,
	DirectX::XMFLOAT3 positionRandomRange,
	DirectX::XMFLOAT2 rotationStartMinMax,
	DirectX::XMFLOAT2 rotationEndMinMax,
	DirectX::XMFLOAT3 emitterAcceleration,
	unsigned int textureDescriptorIndex,
	unsigned int spriteSheetWidth,
	unsigned int spriteSheetHeight,
	float spriteSheetSpeedScale,
	bool paused,
	bool visible) 
	:
	textureDescriptorIndex(textureDescriptorIndex),
	maxParticles(maxParticles),
	particlesPerSecond(particlesPerSecond),
	secondsPerParticle(1.0f / particlesPerSecond),
	lifetime(lifetime),
	startSize(startSize),
	endSize(endSize),
	startColor(startColor),
	endColor(endColor),
	constrainYAxis(constrainYAxis),
	positionRandomRange(positionRandomRange),
	startVelocity(startVelocity),
	velocityRandomRange(velocityRandomRange),
	emitterAcceleration(emitterAcceleration),
	rotationStartMinMax(rotationStartMinMax),
	rotationEndMinMax(rotationEndMinMax),
	spriteSheetWidth(max(spriteSheetWidth, 1)),
	spriteSheetHeight(max(spriteSheetHeight, 1)),
	spriteSheetFrameWidth(1.0f / spriteSheetWidth),
	spriteSheetFrameHeight(1.0f / spriteSheetHeight),
	spriteSheetSpeedScale(spriteSheetSpeedScale),
	paused(paused),
	visible(visible),
	particles(0),
	totalEmitterTime(0)
{
	transform = std::make_shared<Transform>();
	transform->SetPosition(emitterPosition);

	// Set up emission and lifetime stats
	timeSinceLastEmit = 0.0f;
	livingParticleCount = 0;
	firstAliveIndex = 0;
	firstDeadIndex = 0;

	// Actually create the array and underlying GPU resources
	CreateParticlesAndGPUResources();

	CreateRootSigAndPipelineState();
}

Emitter::~Emitter()
{
	// Clean up the particle array
	delete[] particles;
}

std::shared_ptr<Transform> Emitter::GetTransform() { return transform; }

void Emitter::CreateParticlesAndGPUResources()
{
	// Delete and release existing resources
	if (particles) delete[] particles;
	indexBuffer.Reset();

	// Set up the particle array
	particles = new Particle[maxParticles];
	ZeroMemory(particles, sizeof(Particle) * maxParticles);

	// Create an index buffer for particle drawing
	// indices as if we had two triangles per particle
	int numIndices = maxParticles * 6;
	unsigned int* indices = new unsigned int[numIndices];
	int indexCount = 0;
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		indices[indexCount++] = i;
		indices[indexCount++] = i + 1;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i + 3;
	}

	// Create the buffer on the GPU and delete the local array
	indexBuffer = Graphics::CreateStaticBuffer(sizeof(unsigned int), numIndices, indices);
	delete[] indices;

	// Create an index buffer view
	ibv.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	ibv.SizeInBytes = (UINT)(sizeof(unsigned int) * numIndices);
	ibv.Format = DXGI_FORMAT_R32_UINT;

	// Describe the upload buffers to hold particle data on the GPU
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // Upload heap since we'll be copying often!
	heapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Alignment = 0;
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.Height = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.Width = sizeof(Particle) * maxParticles;

	// Make a number of buffers and associated descriptors for each
	// possible frame in flight
	for (unsigned int i = 0; i < Graphics::NumBackBuffers; i++)
	{
		// Reset if necessary
		particleDataBuffer[i].Reset();

		// Create the buffer
		Graphics::Device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			0,
			IID_PPV_ARGS(particleDataBuffer[i].GetAddressOf()));

		// Keep mapped!
		D3D12_RANGE range{ 0, 0 };
		particleDataBuffer[i]->Map(0, &range, &particleDataBufferAddress[i]);

		// Do we need to reserve a descriptor?
		if (particleDataGPUHandle[i].ptr == 0)
			Graphics::ReserveDescriptorHeapSlot(&particleDataCPUHandle[i], &particleDataGPUHandle[i]);
		
		// Create the SRV for the buffer
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = maxParticles;
		srvDesc.Buffer.StructureByteStride = sizeof(Particle);
		Graphics::Device->CreateShaderResourceView(particleDataBuffer[i].Get(), &srvDesc, particleDataCPUHandle[i]);
	}
}

void Emitter::CreateRootSigAndPipelineState()
{
	// Load shaders
	Microsoft::WRL::ComPtr<ID3DBlob> vs;
	Microsoft::WRL::ComPtr<ID3DBlob> ps;
	D3DReadFileToBlob(FixPath(L"ParticleVS.cso").c_str(), vs.GetAddressOf());
	D3DReadFileToBlob(FixPath(L"particlePS.cso").c_str(), ps.GetAddressOf());

	// Root Signature
	{
		// Create the root parameters
		D3D12_ROOT_PARAMETER rootParams[1] = {};

		// Root params for descriptor indices
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[0].Constants.Num32BitValues = sizeof(ParticleDrawData) / sizeof(unsigned int);
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
		D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
		rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
		rootSigDesc.NumParameters = ARRAYSIZE(rootParams);
		rootSigDesc.pParameters = rootParams;
		rootSigDesc.NumStaticSamplers = ARRAYSIZE(samplers);
		rootSigDesc.pStaticSamplers = samplers;

		ID3DBlob* serializedRootSig = 0;
		ID3DBlob* errors = 0;

		D3D12SerializeRootSignature(
			&rootSigDesc,
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
			IID_PPV_ARGS(rootSig.GetAddressOf()));
	}

	// Pipeline state
	{
		// Describe the pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

		// -- Input assembler related ---
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// Root sig
		psoDesc.pRootSignature = rootSig.Get();

		// -- Shaders (VS/PS) --- 
		psoDesc.VS.pShaderBytecode = vs->GetBufferPointer();
		psoDesc.VS.BytecodeLength = vs->GetBufferSize();
		psoDesc.PS.pShaderBytecode = ps->GetBufferPointer();
		psoDesc.PS.BytecodeLength = ps->GetBufferSize();

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

		// No depth writing
		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		// Additive blending
		psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
		psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		// -- Misc ---
		psoDesc.SampleMask = 0xffffffff;

		// Create the pipe state object
		Graphics::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pso.GetAddressOf()));

		// Make a "wireframe" version, too
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
		Graphics::Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(psoWireframe.GetAddressOf()));
	}
}

void Emitter::Update(float dt, float currentTime)
{
	if (paused)
		return;

	// Add to the time
	timeSinceLastEmit += dt;
	totalEmitterTime += dt;

	// Anything to update?
	if (livingParticleCount > 0)
	{
		// Update all particles - Check cyclic buffer first
		if (firstAliveIndex < firstDeadIndex)
		{
			// First alive is BEFORE first dead, so the "living" particles are contiguous
			// 
			// 0 -------- FIRST ALIVE ----------- FIRST DEAD -------- MAX
			// |    dead    |            alive       |         dead    |

			// First alive is before first dead, so no wrapping
			for (int i = firstAliveIndex; i < firstDeadIndex; i++)
				UpdateSingleParticle(totalEmitterTime, i);
		}
		else if (firstDeadIndex < firstAliveIndex)
		{
			// First alive is AFTER first dead, so the "living" particles wrap around
			// 
			// 0 -------- FIRST DEAD ----------- FIRST ALIVE -------- MAX
			// |    alive    |            dead       |         alive   |

			// Update first half (from firstAlive to max particles)
			for (int i = firstAliveIndex; i < maxParticles; i++)
				UpdateSingleParticle(totalEmitterTime, i);

			// Update second half (from 0 to first dead)
			for (int i = 0; i < firstDeadIndex; i++)
				UpdateSingleParticle(totalEmitterTime, i);
		}
		else
		{
			// First alive is EQUAL TO first dead, so they're either all alive or all dead
			// - Since we know at least one is alive, they should all be
			//
			//            FIRST ALIVE
			// 0 -------- FIRST DEAD -------------------------------- MAX
			// |    alive     |                   alive                |
			for (int i = 0; i < maxParticles; i++)
				UpdateSingleParticle(totalEmitterTime, i);
		}
	}


	// Enough time to emit?
	while (timeSinceLastEmit > secondsPerParticle)
	{
		EmitParticle(totalEmitterTime);
		timeSinceLastEmit -= secondsPerParticle;
	}
}


void Emitter::UpdateSingleParticle(float currentTime, int index)
{
	float age = currentTime - particles[index].EmitTime;

	// Update and check for death
	if (age >= lifetime)
	{
		// Recent death, so retire by moving alive count (and wrap)
		firstAliveIndex++;
		firstAliveIndex %= maxParticles;
		livingParticleCount--;
	}
}

void Emitter::EmitParticle(float currentTime)
{
	// Any left to spawn?
	if (livingParticleCount == maxParticles)
		return;

	// Which particle is spawning?
	int spawnedIndex = firstDeadIndex;

	// Update the spawn time
	particles[spawnedIndex].EmitTime = currentTime;

	// Adjust the particle start position based on the random range (box shape)
	particles[spawnedIndex].StartPosition = transform->GetPosition();
	particles[spawnedIndex].StartPosition.x += positionRandomRange.x * RandomRange(-1.0f, 1.0f);
	particles[spawnedIndex].StartPosition.y += positionRandomRange.y * RandomRange(-1.0f, 1.0f);
	particles[spawnedIndex].StartPosition.z += positionRandomRange.z * RandomRange(-1.0f, 1.0f);

	// Adjust particle start velocity based on random range
	particles[spawnedIndex].StartVelocity = startVelocity;
	particles[spawnedIndex].StartVelocity.x += velocityRandomRange.x * RandomRange(-1.0f, 1.0f);
	particles[spawnedIndex].StartVelocity.y += velocityRandomRange.y * RandomRange(-1.0f, 1.0f);
	particles[spawnedIndex].StartVelocity.z += velocityRandomRange.z * RandomRange(-1.0f, 1.0f);

	// Adjust start and end rotation values based on range
	particles[spawnedIndex].StartRotation = RandomRange(rotationStartMinMax.x, rotationStartMinMax.y);
	particles[spawnedIndex].EndRotation = RandomRange(rotationEndMinMax.x, rotationEndMinMax.y);

	// Increment the first dead particle (since it's now alive)
	firstDeadIndex++;
	firstDeadIndex %= maxParticles; // Wrap

	// One more living particle
	livingParticleCount++;
}

void Emitter::Draw(std::shared_ptr<Camera> camera, float currentTime, bool debugWireframe)
{
	if (!visible)
		return;

	CopyParticlesToGPU();

	// Set render details
	Graphics::CommandList->SetPipelineState(debugWireframe ? psoWireframe.Get() : pso.Get());
	Graphics::CommandList->SetGraphicsRootSignature(rootSig.Get());

	// Overall draw data
	ParticleDrawData drawData{};
	drawData.DebugWireframe = debugWireframe;
	drawData.ParticleTextureIndex = textureDescriptorIndex;
	drawData.ParticleDataIndex = Graphics::GetDescriptorIndex(particleDataGPUHandle[Graphics::SwapChainIndex()]);

	// Set up VS constant buffer data
	{
		ParticleVSConstantBuffer cb{};
		cb.View = camera->GetView();
		cb.Projection = camera->GetProjection();
		cb.CurrentTime = totalEmitterTime;
		cb.Lifetime = lifetime;
		cb.Acceleration = emitterAcceleration;
		cb.StartSize = startSize;
		cb.EndSize = endSize;
		cb.StartColor = startColor;
		cb.EndColor = endColor;
		cb.ColorTint = XMFLOAT3(1, 1, 1);
		cb.ConstrainYAxis = constrainYAxis;
		cb.SpriteSheetWidth = spriteSheetWidth;
		cb.SpriteSheetHeight = spriteSheetHeight;
		cb.SpriteSheetFrameWidth = spriteSheetFrameWidth;
		cb.SpriteSheetFrameHeight = spriteSheetFrameHeight;
		cb.SpriteSheetSpeedScale = spriteSheetSpeedScale;

		D3D12_GPU_DESCRIPTOR_HANDLE cbHandle = Graphics::FillNextConstantBufferAndGetGPUDescriptorHandle(
			(void*)(&cb), sizeof(ParticleVSConstantBuffer));

		// Set the index in draw data
		drawData.ParticleCBIndex = Graphics::GetDescriptorIndex(cbHandle);
	}

	// Set the basic draw data
	Graphics::CommandList->SetGraphicsRoot32BitConstants(
		0,
		sizeof(ParticleDrawData) / sizeof(unsigned int),
		&drawData,
		0);

	// Now that all of our data is in the beginning of the particle buffer,
	// we can simply draw the correct amount of living particle indices.
	// Each particle = 4 vertices = 6 indices for a quad
	Graphics::CommandList->IASetIndexBuffer(&ibv);
	Graphics::CommandList->DrawIndexedInstanced(livingParticleCount * 6, 1, 0, 0, 0);
}

void Emitter::CopyParticlesToGPU()
{
	// Now that we have emit and updated all particles for this frame, 
	// we can copy them to the GPU as either one big chunk or two smaller chunks

	// Which frame are we on?
	unsigned int frame = Graphics::SwapChainIndex();

	// How are living particles arranged in the buffer?
	if (firstAliveIndex < firstDeadIndex)
	{
		// Only copy from FirstAlive -> FirstDead
		memcpy(
			particleDataBufferAddress[frame], // Destination = start of particle buffer
			particles + firstAliveIndex, // Source = particle array, offset to first living particle
			sizeof(Particle) * livingParticleCount); // Amount = number of particles (measured in BYTES!)
	}
	else
	{
		// Copy from 0 -> FirstDead 
		memcpy(
			particleDataBufferAddress[frame], // Destination = start of particle buffer
			particles, // Source = start of particle array
			sizeof(Particle) * firstDeadIndex); // Amount = particles up to first dead (measured in BYTES!)

		// ALSO copy from FirstAlive -> End
		memcpy(
			(void*)((Particle*)particleDataBufferAddress[frame] + firstDeadIndex), // Destination = particle buffer, AFTER the data we copied in previous memcpy()
			particles + firstAliveIndex,  // Source = particle array, offset to first living particle
			sizeof(Particle) * (maxParticles - firstAliveIndex)); // Amount = number of living particles at end of array (measured in BYTES!)
	}
}

int Emitter::GetParticlesPerSecond()
{
	return particlesPerSecond;
}

void Emitter::SetParticlesPerSecond(int particlesPerSecond)
{
	this->particlesPerSecond = max(1, particlesPerSecond);
	this->secondsPerParticle = 1.0f / particlesPerSecond;
}

int Emitter::GetMaxParticles()
{
	return maxParticles;
}

void Emitter::SetMaxParticles(int maxParticles)
{
	this->maxParticles = max(1, maxParticles);
	CreateParticlesAndGPUResources();

	// Reset emission details
	timeSinceLastEmit = 0.0f;
	livingParticleCount = 0;
	firstAliveIndex = 0;
	firstDeadIndex = 0;
}

bool Emitter::IsSpriteSheet()
{
	return spriteSheetHeight > 1 || spriteSheetWidth > 1;
}

