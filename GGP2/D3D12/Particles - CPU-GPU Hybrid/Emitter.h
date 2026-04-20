#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>

#include "Graphics.h"
#include "Camera.h"
#include "Transform.h"

// We'll be mimicking this in HLSL
// so we need to care about alignment!
struct Particle
{
	float EmitTime;
	DirectX::XMFLOAT3 StartPosition;

	DirectX::XMFLOAT3 StartVelocity;
	float StartRotation;

	float EndRotation;
	DirectX::XMFLOAT3 pad;
};

struct ParticleVSConstantBuffer
{
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT4X4 Projection;

	DirectX::XMFLOAT4 StartColor;
	DirectX::XMFLOAT4 EndColor;

	float CurrentTime;
	DirectX::XMFLOAT3 Acceleration;

	int SpriteSheetWidth;
	int SpriteSheetHeight;
	float SpriteSheetFrameWidth;
	float SpriteSheetFrameHeight;

	float SpriteSheetSpeedScale;
	float StartSize;
	float EndSize;
	float Lifetime;

	DirectX::XMFLOAT3 ColorTint;
	int ConstrainYAxis;
};

struct ParticleDrawData
{
	unsigned int ParticleCBIndex;
	unsigned int ParticleDataIndex;
	unsigned int ParticleTextureIndex;
	unsigned int DebugWireframe;
};

class Emitter
{
public:
	Emitter(
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
		unsigned int spriteSheetWidth = 1,
		unsigned int spriteSheetHeight = 1,
		float spriteSheetSpeedScale = 1.0f,
		bool paused = false,
		bool visible = true
	);
	~Emitter();

	void Update(float dt, float currentTime);
	void Draw(
		std::shared_ptr<Camera> camera,
		float currentTime,
		bool debugWireframe);

	std::shared_ptr<Transform> GetTransform();

	// Lifetime and emission
	float lifetime;
	int GetParticlesPerSecond();
	void SetParticlesPerSecond(int particlesPerSecond);
	int GetMaxParticles();
	void SetMaxParticles(int maxParticles);

	// Emitter-level data (this is the same for all particles)
	DirectX::XMFLOAT3 emitterAcceleration;
	DirectX::XMFLOAT3 startVelocity;

	// Particle visual data (interpolated
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	float startSize;
	float endSize;
	bool constrainYAxis;
	bool paused;
	bool visible;

	// Particle randomization ranges
	DirectX::XMFLOAT3 positionRandomRange;
	DirectX::XMFLOAT3 velocityRandomRange;
	DirectX::XMFLOAT2 rotationStartMinMax;
	DirectX::XMFLOAT2 rotationEndMinMax;

	// Sprite sheet animation
	float spriteSheetSpeedScale;
	bool IsSpriteSheet();

private:

	// Emission
	int maxParticles;
	int particlesPerSecond;
	float secondsPerParticle;
	float timeSinceLastEmit;
	float totalEmitterTime;

	// Sprite sheet options
	unsigned int textureDescriptorIndex;
	int spriteSheetWidth;
	int spriteSheetHeight;
	float spriteSheetFrameWidth;
	float spriteSheetFrameHeight;

	// Particle array
	Particle* particles;
	int firstDeadIndex;
	int firstAliveIndex;
	int livingParticleCount;
	void CreateParticlesAndGPUResources();
	void CreateRootSigAndPipelineState();

	// Rendering
	Microsoft::WRL::ComPtr<ID3D12Resource> particleDataBuffer[Graphics::NumBackBuffers];
	void* particleDataBufferAddress[Graphics::NumBackBuffers]{};
	D3D12_CPU_DESCRIPTOR_HANDLE particleDataCPUHandle[Graphics::NumBackBuffers]{};
	D3D12_GPU_DESCRIPTOR_HANDLE particleDataGPUHandle[Graphics::NumBackBuffers]{};
	
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
	D3D12_INDEX_BUFFER_VIEW ibv;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> psoWireframe;

	// Material & transform
	std::shared_ptr<Transform> transform;

	// Creation and copy methods
	void CopyParticlesToGPU();

	// Simulation methods
	void UpdateSingleParticle(float currentTime, int index);
	void EmitParticle(float currentTime);
};

