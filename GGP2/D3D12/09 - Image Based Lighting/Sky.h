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

struct BrdfLUTComputeIndices
{
	unsigned int OutputWidth;
	unsigned int OutputHeight;
	unsigned int OutputDescriptorIndex;
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
	unsigned int GetBrdfLookUpTableDescriptorIndex();

private:

	void InitRenderStates();

	// Skybox related resources
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	
	std::shared_ptr<Mesh> skyMesh;

	unsigned int skyboxDescriptorIndex;

	// Compute pipeline for IBL preprocessing
	const int BrdfLookUpTableSize = 512;

	void CreateIBLResources();
	void CreateIBLBrdfLookUpTable();
	void CreateIBLSpecularMap();
	void CreateIBLIrradianceMap();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSig;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> brdfLookUpTablePSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> specularMapPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> irradianceMapPSO;

	Microsoft::WRL::ComPtr<ID3D12Resource> brdfLookUpTable;
	Microsoft::WRL::ComPtr<ID3D12Resource> specularMap;
	Microsoft::WRL::ComPtr<ID3D12Resource> irradianceMap;

	unsigned int brdfLookUpTableDescriptorIndex = -1;
	unsigned int specularMapDescriptorIndex = -1;
	unsigned int irradianceMapDescriptorIndex = -1;
};

