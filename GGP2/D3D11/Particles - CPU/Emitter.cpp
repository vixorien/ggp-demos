#include "Emitter.h"
#include "Graphics.h"

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
	std::shared_ptr<Material> material,
	unsigned int spriteSheetWidth,
	unsigned int spriteSheetHeight,
	float spriteSheetSpeedScale,
	bool paused,
	bool visible) :
		material(material),
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
		localParticleVertices(0)
{
	transform = std::make_shared<Transform>();
	this->transform->SetPosition(emitterPosition);

	// Set up emission and lifetime stats
	timeSinceLastEmit = 0;
	livingParticleCount = 0;
	firstAliveIndex = 0;
	firstDeadIndex = 0;

	// Set up Default UVs
	DefaultUVs[0] = XMFLOAT2(0, 0);
	DefaultUVs[1] = XMFLOAT2(1, 0);
	DefaultUVs[2] = XMFLOAT2(1, 1);
	DefaultUVs[3] = XMFLOAT2(0, 1);

	// Actually create the array and underlying GPU resources
	CreateParticlesAndGPUResources();
}


Emitter::~Emitter()
{
	delete[] particles;
	delete[] localParticleVertices;
}

std::shared_ptr<Transform> Emitter::GetTransform() { return transform; }
std::shared_ptr<Material> Emitter::GetMaterial() { return material; }
void Emitter::SetMaterial(std::shared_ptr<Material> material) { this->material = material; }


void Emitter::CreateParticlesAndGPUResources()
{
	// Delete and release existing resources
	if (particles) delete[] particles;
	if (localParticleVertices) delete[] localParticleVertices;
	indexBuffer.Reset();
	vertexBuffer.Reset();

	// Set up the particle array
	particles = new Particle[maxParticles];
	ZeroMemory(particles, sizeof(Particle) * maxParticles);

	// Create UV's here, as those will usually stay the same
	localParticleVertices = new ParticleVertex[4 * maxParticles];
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		localParticleVertices[i + 0].UV = DefaultUVs[0];
		localParticleVertices[i + 1].UV = DefaultUVs[1];
		localParticleVertices[i + 2].UV = DefaultUVs[2];
		localParticleVertices[i + 3].UV = DefaultUVs[3];
	}

	// DYNAMIC vertex buffer (no initial data necessary)
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vbDesc.Usage = D3D11_USAGE_DYNAMIC;
	vbDesc.ByteWidth = sizeof(ParticleVertex) * 4 * maxParticles;
	Graphics::Device->CreateBuffer(&vbDesc, 0, vertexBuffer.GetAddressOf());

	// Index buffer data
	unsigned int* indices = new unsigned int[maxParticles * 6];
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
	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices;

	// Regular (static) index buffer
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = 0;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(unsigned int) * maxParticles * 6;
	Graphics::Device->CreateBuffer(&ibDesc, &indexData, indexBuffer.GetAddressOf());

	delete[] indices;
}



void Emitter::Update(float dt)
{
	if (paused)
		return;

	// Update all particles - Check cyclic buffer first
	if (firstAliveIndex < firstDeadIndex)
	{
		// First alive is BEFORE first dead, so the "living" particles are contiguous
		// 
		// 0 -------- FIRST ALIVE ----------- FIRST DEAD -------- MAX
		// |    dead    |            alive       |         dead    |

		// First alive is before first dead, so no wrapping
		for (int i = firstAliveIndex; i < firstDeadIndex; i++)
			UpdateSingleParticle(dt, i);
	}
	else
	{
		// First alive is AFTER first dead, so the "living" particles wrap around
		// 
		// 0 -------- FIRST DEAD ----------- FIRST ALIVE -------- MAX
		// |    alive    |            dead       |         alive   |

		// Update first half (from firstAlive to max particles)
		for (int i = firstAliveIndex; i < maxParticles; i++)
			UpdateSingleParticle(dt, i);

		// Update second half (from 0 to first dead)
		for (int i = 0; i < firstDeadIndex; i++)
			UpdateSingleParticle(dt, i);
	}

	// Add to the time
	timeSinceLastEmit += dt;

	// Enough time to emit?
	while (timeSinceLastEmit > secondsPerParticle)
	{
		SpawnParticle();
		timeSinceLastEmit -= secondsPerParticle;
	}
}

void Emitter::UpdateSingleParticle(float dt, int index)
{
	// Check for valid particle age before doing anything
	if (particles[index].Age >= lifetime)
		return;

	// Update and check for death
	particles[index].Age += dt;
	if (particles[index].Age >= lifetime)
	{
		// Recent death, so retire by moving alive count
		firstAliveIndex++;
		firstAliveIndex %= maxParticles;
		livingParticleCount--;
		return;
	}

	// Calculate age percentage for lerp
	float agePercent = particles[index].Age / lifetime;

	// Interpolate the color
	XMStoreFloat4(
		&particles[index].Color,
		XMVectorLerp(
			XMLoadFloat4(&startColor),
			XMLoadFloat4(&endColor),
			agePercent));

	// Interpolate the rotation
	float rotStart = particles[index].RotationStart;
	float rotEnd = particles[index].RotationEnd;
	particles[index].Rotation = rotStart + agePercent * (rotEnd - rotStart);

	// Interpolate the size
	particles[index].Size = startSize + agePercent * (endSize - startSize);

	// Adjust the position
	XMVECTOR startPos = XMLoadFloat3(&particles[index].StartPosition);
	XMVECTOR startVel = XMLoadFloat3(&particles[index].StartVelocity);
	XMVECTOR accel = XMLoadFloat3(&emitterAcceleration);
	float t = particles[index].Age;

	// Use constant acceleration function
	XMStoreFloat3(
		&particles[index].Position,
		accel * t * t / 2.0f + startVel * t + startPos);
}

void Emitter::SpawnParticle()
{
	// Any left to spawn?
	if (livingParticleCount == maxParticles)
		return;

	// Reset the first dead particle
	particles[firstDeadIndex].Age = 0;
	particles[firstDeadIndex].Size = startSize;
	particles[firstDeadIndex].Color = startColor;

	particles[firstDeadIndex].StartPosition = XMFLOAT3(0,0,0);
	particles[firstDeadIndex].StartPosition.x += (((float)rand() / RAND_MAX) * 2 - 1) * positionRandomRange.x;
	particles[firstDeadIndex].StartPosition.y += (((float)rand() / RAND_MAX) * 2 - 1) * positionRandomRange.y;
	particles[firstDeadIndex].StartPosition.z += (((float)rand() / RAND_MAX) * 2 - 1) * positionRandomRange.z;

	particles[firstDeadIndex].Position = particles[firstDeadIndex].StartPosition;

	particles[firstDeadIndex].StartVelocity = startVelocity;
	particles[firstDeadIndex].StartVelocity.x += (((float)rand() / RAND_MAX) * 2 - 1) * velocityRandomRange.x;
	particles[firstDeadIndex].StartVelocity.y += (((float)rand() / RAND_MAX) * 2 - 1) * velocityRandomRange.y;
	particles[firstDeadIndex].StartVelocity.z += (((float)rand() / RAND_MAX) * 2 - 1) * velocityRandomRange.z;

	float rotStartMin = rotationStartMinMax.x;
	float rotStartMax = rotationStartMinMax.y;
	particles[firstDeadIndex].RotationStart = ((float)rand() / RAND_MAX) * (rotStartMax - rotStartMin) + rotStartMin;

	float rotEndMin = rotationEndMinMax.x;
	float rotEndMax = rotationEndMinMax.y;
	particles[firstDeadIndex].RotationEnd = ((float)rand() / RAND_MAX) * (rotEndMax - rotEndMin) + rotEndMin;

	// Increment and wrap
	firstDeadIndex++;
	firstDeadIndex %= maxParticles;

	livingParticleCount++;
}

void Emitter::CopyParticlesToGPU(std::shared_ptr<Camera> camera)
{
	// Update local buffer (living particles only as a speed up)

	// Check cyclic buffer status
	if (firstAliveIndex < firstDeadIndex)
	{
		for (int i = firstAliveIndex; i < firstDeadIndex; i++)
			CopyOneParticle(i, camera);
	}
	else
	{
		// Update first half (from firstAlive to max particles)
		for (int i = firstAliveIndex; i < maxParticles; i++)
			CopyOneParticle(i, camera);

		// Update second half (from 0 to first dead)
		for (int i = 0; i < firstDeadIndex; i++)
			CopyOneParticle(i, camera);
	}

	// All particles copied locally - send whole buffer to GPU
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	Graphics::Context->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	memcpy(mapped.pData, localParticleVertices, sizeof(ParticleVertex) * 4 * maxParticles);

	Graphics::Context->Unmap(vertexBuffer.Get(), 0);
}

void Emitter::CopyOneParticle(int index, std::shared_ptr<Camera> camera)
{
	int i = index * 4;

	localParticleVertices[i + 0].Position = CalcParticleVertexPosition(index, 0, camera);
	localParticleVertices[i + 1].Position = CalcParticleVertexPosition(index, 1, camera);
	localParticleVertices[i + 2].Position = CalcParticleVertexPosition(index, 2, camera);
	localParticleVertices[i + 3].Position = CalcParticleVertexPosition(index, 3, camera);

	localParticleVertices[i + 0].Color = particles[index].Color;
	localParticleVertices[i + 1].Color = particles[index].Color;
	localParticleVertices[i + 2].Color = particles[index].Color;
	localParticleVertices[i + 3].Color = particles[index].Color;

	// If it's a spritesheet, we need to update UV coords as the particle ages
	if (IsSpriteSheet())
	{
		// How old is this particle as a percentage
		float agePercent = particles[index].Age / lifetime;

		// Which overall index?
		int ssIndex = (int)floor(agePercent * (spriteSheetWidth * spriteSheetHeight));

		// Get the U/V indices (basically column & row index across the sprite sheet)
		int uIndex = ssIndex % spriteSheetWidth;
		int vIndex = ssIndex / spriteSheetWidth; // Integer division is important here!

		// Convert to a top-left corner in uv space (0-1)
		float u = uIndex / (float)spriteSheetWidth;
		float v = vIndex / (float)spriteSheetHeight;

		localParticleVertices[i + 0].UV = XMFLOAT2(u, v);
		localParticleVertices[i + 1].UV = XMFLOAT2(u + spriteSheetFrameWidth, v);
		localParticleVertices[i + 2].UV = XMFLOAT2(u + spriteSheetFrameWidth, v + spriteSheetFrameHeight);
		localParticleVertices[i + 3].UV = XMFLOAT2(u, v + spriteSheetFrameHeight);
	}
}


XMFLOAT3 Emitter::CalcParticleVertexPosition(int particleIndex, int quadCornerIndex, std::shared_ptr<Camera> camera)
{
	// Get the right and up vectors out of the view matrix
	XMFLOAT4X4 view = camera->GetView();
	XMVECTOR camRight = XMVectorSet(view._11, view._21, view._31, 0);
	XMVECTOR camUp = constrainYAxis ? XMVectorSet(0,1,0,0) : XMVectorSet(view._12, view._22, view._32, 0);

	// Determine the offset of this corner of the quad
	// Since the UV's are already set when the emitter is created, 
	// we can alter that data to determine the general offset of this corner
	XMFLOAT2 offset = DefaultUVs[quadCornerIndex];
	offset.x = offset.x * 2 - 1;	// Convert from [0,1] to [-1,1]
	offset.y = (offset.y * -2 + 1);	// Same, but flip the Y

	// Load into a vector, which we'll assume is float3 with a Z of 0
	// Create a Z rotation matrix and apply it to the offset
	XMVECTOR offsetVec = XMLoadFloat2(&offset);
	XMMATRIX rotMatrix = XMMatrixRotationZ(particles[particleIndex].Rotation);
	offsetVec = XMVector3Transform(offsetVec, rotMatrix);

	// Add and scale the camera up/right vectors to the position as necessary
	XMVECTOR posVec = XMLoadFloat3(&particles[particleIndex].Position);
	posVec += camRight * XMVectorGetX(offsetVec) * particles[particleIndex].Size;
	posVec += camUp * XMVectorGetY(offsetVec) * particles[particleIndex].Size;

	// This position is all set
	XMFLOAT3 pos;
	XMStoreFloat3(&pos, posVec);
	return pos;
}



void Emitter::Draw(std::shared_ptr<Camera> camera, bool debugWireframe)
{
	if (!visible)
		return;

	// Copy to dynamic buffer
	CopyParticlesToGPU(camera);

	// Set up buffers
	UINT stride = sizeof(ParticleVertex);
	UINT offset = 0;
	Graphics::Context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
	Graphics::Context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	// Set particle-specific data and let the
	// material take care of the rest
	material->GetPixelShader()->SetInt("debugWireframe", (int)debugWireframe);
	material->PrepareMaterial(transform, camera);

	// Draw the correct parts of the buffer
	if (firstAliveIndex < firstDeadIndex)
	{
		Graphics::Context->DrawIndexed(livingParticleCount * 6, firstAliveIndex * 6, 0);
	}
	else
	{
		// Draw first half (0 -> dead)
		Graphics::Context->DrawIndexed(firstDeadIndex * 6, 0, 0);

		// Draw second half (alive -> max)
		Graphics::Context->DrawIndexed((maxParticles - firstAliveIndex) * 6, firstAliveIndex * 6, 0);
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