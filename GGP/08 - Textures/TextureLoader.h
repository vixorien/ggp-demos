#pragma once

#include <Windows.h>

#include <d3d11.h>
#include <wrl/client.h>

HRESULT LoadTexture(
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
	const wchar_t* imageFile,
	ID3D11Texture2D** texture,
	ID3D11ShaderResourceView** srv,
	bool generateMips = true);

void GetFormatConversionDetails(
	GUID wic,
	GUID* wicConvert,
	DXGI_FORMAT* dxgi,
	unsigned int* bpp);