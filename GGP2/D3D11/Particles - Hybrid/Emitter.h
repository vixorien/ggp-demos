#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>

#include "Camera.h"
#include "Material.h"
#include "Transform.h"
#include "SimpleShader.h"

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
		std::shared_ptr<Material> material,
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
	std::shared_ptr<Material> GetMaterial();
	void SetMaterial(std::shared_ptr<Material> material);

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

	// Rendering
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDataSRV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	// Material & transform
	std::shared_ptr<Transform> transform;
	std::shared_ptr<Material> material;

	// Creation and copy methods
	void CopyParticlesToGPU();

	// Simulation methods
	void UpdateSingleParticle(float currentTime, int index);
	void EmitParticle(float currentTime);
};

