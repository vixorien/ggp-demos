#pragma once

#include "Mesh.h"
#include "SimpleShader.h"
#include "Camera.h"

#include <memory>
#include <wrl/client.h> // Used for ComPtr

struct IBLOptions
{
	bool IndirectLightingEnabled;
	std::shared_ptr<SimpleVertexShader> FullscreenVS;
	std::shared_ptr<SimplePixelShader> IBLIrradiancePS;
	std::shared_ptr<SimplePixelShader> IBLSpecularConvolutionPS;
	std::shared_ptr<SimplePixelShader> IBLBRDFLookUpPS;

	// SRVs for debug drawing
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> BRDFLookUpSRV;
};

class Sky
{
public:

	// Constructor that takes an existing cube map SRV
	Sky(
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubeMap,
		std::shared_ptr<Mesh> mesh,
		std::shared_ptr<SimpleVertexShader> skyVS,
		std::shared_ptr<SimplePixelShader> skyPS,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
		IBLOptions& iblOptions
	);

	// Constructor that loads a DDS cube map file
	Sky(
		const wchar_t* cubemapDDSFile, 
		std::shared_ptr<Mesh> mesh,
		std::shared_ptr<SimpleVertexShader> skyVS,
		std::shared_ptr<SimplePixelShader> skyPS,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
		IBLOptions& iblOptions
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
		std::shared_ptr<SimpleVertexShader> skyVS,
		std::shared_ptr<SimplePixelShader> skyPS,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions,
		IBLOptions& iblOptions
	);

	~Sky();

	void Draw(std::shared_ptr<Camera> camera);

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSkyTexture();

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetIrradianceIBLMap();
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSpecularIBLMap();
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetBRDFLookUpTexture();
	int GetTotalSpecularIBLMipLevels();

private:

	void InitRenderStates();

	// Helper for creating a cubemap from 6 individual textures
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateCubemap(
		const wchar_t* right,
		const wchar_t* left,
		const wchar_t* up,
		const wchar_t* down,
		const wchar_t* front,
		const wchar_t* back);

	// Skybox related resources
	std::shared_ptr<SimpleVertexShader> skyVS;
	std::shared_ptr<SimplePixelShader> skyPS;
	
	std::shared_ptr<Mesh> skyMesh;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> skyRasterState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> skyDepthState;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> skySRV;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> clampSampler;

	// Image-Based Lighting (IBL) resources and options
	const int IBLCubeSize = 256;
	const int IBLLookUpTextureSize = 256;
	const int specIBLMipLevelsToSkip = 3; // Number of lower mips (1x1, 2x2, etc.) to exclude from the maps
	int totalSpecIBLMipLevels;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> irradianceIBL;		// Incoming diffuse light
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specularIBL;		// Incoming specular light
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> brdfLookUpMap;		// Holds some pre-calculated BRDF results

	// IBL precompute steps
	void IBLCreateIrradianceMap(IBLOptions& iblOptions);
	void IBLCreateConvolvedSpecularMap(IBLOptions& iblOptions);
	void IBLCreateBRDFLookUpTexture(IBLOptions& iblOptions);
};

