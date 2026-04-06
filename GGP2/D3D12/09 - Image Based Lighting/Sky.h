#pragma once

#include "Mesh.h"
#include "Camera.h"
#include "Graphics.h"

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

struct IrradianceComputeIndices
{
	unsigned int OutputWidth;
	unsigned int OutputHeight;
	unsigned int OutputDescriptorIndex;
	unsigned int EnvironmentMapDescriptorIndex;
};

struct SpecularComputeIndices
{
	unsigned int OutputWidth;
	unsigned int OutputHeight;
	unsigned int OutputDescriptorIndex;
	unsigned int EnvironmentMapDescriptorIndex;
	unsigned int MipLevel;
	float Roughness;
};

class Sky
{
public:

	// Constructor that takes an existing cube map SRV
	Sky(
		TextureDetails skyCubeDetails,
		std::shared_ptr<Mesh> mesh,
		bool useSphericalHarmonicsForIrradiance = false
	);

	// Constructor that loads a DDS cube map file
	Sky(
		const wchar_t* cubemapDDSFile, 
		std::shared_ptr<Mesh> mesh,
		bool useSphericalHarmonicsForIrradiance = false
	);

	// Constructor that loads 6 textures and makes a cube map
	Sky(
		const wchar_t* right,
		const wchar_t* left,
		const wchar_t* up,
		const wchar_t* down,
		const wchar_t* front,
		const wchar_t* back,
		std::shared_ptr<Mesh> mesh,
		bool useSphericalHarmonicsForIrradiance = false
	);

	// Constructor that loads a cube map and other
	// IBL maps together, rather than making them dynamically
	Sky(
		const wchar_t* cubemapDDSFile,
		const wchar_t* irradianceMapDDSFile,
		const wchar_t* specularMapDDSFile,
		const wchar_t* brdfLookUpTableDDSFile,
		unsigned int totalSpecMipLevels,
		std::shared_ptr<Mesh> mesh
	);

	~Sky();

	void Draw(std::shared_ptr<Camera> camera);

	unsigned int GetSkyboxDescriptorIndex();
	unsigned int GetBrdfLookUpTableDescriptorIndex();
	unsigned int GetIrradianceMapDescriptorIndex();
	unsigned int GetSpecularMapDescriptorIndex();
	unsigned int GetTotalSpecularMipLevels();

private:

	void InitRenderStates();

	// Skybox related resources
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	
	std::shared_ptr<Mesh> skyMesh;

	TextureDetails skyCubeMap;

	// Compute pipeline for IBL preprocessing
	const unsigned int BrdfLookUpTableSize = 512;
	const unsigned int IrradianceMapSize = 512;
	const unsigned int SpecularMapSize = 256;
	const unsigned int SpecMipLevelsToSkip = 3; // Number of lower mips (1x1, 2x2, etc.) to exclude from the maps
	unsigned int totalSpecMipLevels;

	void CreateIBLResources();
	void CreateIBLBrdfLookUpTable();
	void CreateIBLSpecularMap();
	void CreateIBLIrradianceMap();
	void CreateIBLIrradianceSphericalHarmonics();
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSig;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> brdfLookUpTablePSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> specularMapPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> irradianceMapPSO;

	TextureDetails brdfLookUpTable;
	TextureDetails specularMap;
	TextureDetails irradianceMap;

	bool useSphericalHarmonicsForIrradiance;
};

