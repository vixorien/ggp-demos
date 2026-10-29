#pragma once

#include "Mesh.h"
#include "Camera.h"

#include <memory>
#include <wrl/client.h> // Used for ComPtr

struct SkyDrawIndices
{
	unsigned int vsVertexBufferIndex;
	unsigned int vsCBIndex;
	unsigned int psSkyboxIndex;
};

class Sky
{
public:

	// Constructor that takes an existing cube map SRV
	Sky(
		std::shared_ptr<Mesh> mesh,
		unsigned int skyboxDescriptorIndex
	);

	// Constructor that loads a DDS cube map file
	Sky(
		const wchar_t* cubemapDDSFile, 
		std::shared_ptr<Mesh> mesh
	);

	// Constructor that loads 6 textures and makes a cube map
	Sky(
		const wchar_t* right,
		const wchar_t* left,
		const wchar_t* up,
		const wchar_t* down,
		const wchar_t* front,
		const wchar_t* back,
		std::shared_ptr<Mesh> mesh
	);

	~Sky();

	void Draw(std::shared_ptr<Camera> camera);

	unsigned int GetSkyboxDescriptorIndex();

private:

	void InitRenderStates();

	// Helper for creating a cubemap from 6 individual textures
	//Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateCubemap(
	//	const wchar_t* right,
	//	const wchar_t* left,
	//	const wchar_t* up,
	//	const wchar_t* down,
	//	const wchar_t* front,
	//	const wchar_t* back);

	// Skybox related resources
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	
	std::shared_ptr<Mesh> skyMesh;

	unsigned int skyboxDescriptorIndex;
};

