#include "TextureLoader.h"

#include <wincodec.h>

// --------------------------------------------------------
// Gets the format gets based on a given WIC format.
// 
// wic - The WIC format to analyze
// wicConvert - The final WIC format the data should be in
// dxgi - The DXGI format corresponding to the converted WIC format
// bpp - The bits-per-pixel of the final DXGI format
// --------------------------------------------------------
void GetFormatConversionDetails(
	GUID wic, 
	GUID* wicConvert, 
	DXGI_FORMAT* dxgi,
	unsigned int *bpp)
{
#define MATCH(format, wic) memcmp(&format, &wic, sizeof(GUID)) == 0

	// Formats that convert to DXGI_FORMAT_R8G8B8A8_UNORM
	if (MATCH(wic, GUID_WICPixelFormat32bppRGB) ||
		MATCH(wic, GUID_WICPixelFormat32bppRGBA) ||
		MATCH(wic, GUID_WICPixelFormat24bppRGB) ||

		MATCH(wic, GUID_WICPixelFormat32bppPRGBA) ||
		MATCH(wic, GUID_WICPixelFormat32bppPBGRA) || // BGRA -> RGBA
		MATCH(wic, GUID_WICPixelFormat32bppBGRA) ||  // BGRA -> RGBA
		MATCH(wic, GUID_WICPixelFormat32bppBGR) ||   // BGR -> RGBA
		MATCH(wic, GUID_WICPixelFormat24bppBGR) ||   // BGR -> RGBA
		
		MATCH(wic, GUID_WICPixelFormat1bppIndexed) ||
		MATCH(wic, GUID_WICPixelFormat2bppIndexed) ||
		MATCH(wic, GUID_WICPixelFormat4bppIndexed) ||
		MATCH(wic, GUID_WICPixelFormat8bppIndexed) ||
		
		MATCH(wic, GUID_WICPixelFormat32bppCMYK) ||
		MATCH(wic, GUID_WICPixelFormat40bppCMYKAlpha)) 
	{ 
		*wicConvert = GUID_WICPixelFormat32bppRGBA; 
		*dxgi = DXGI_FORMAT_R8G8B8A8_UNORM; 
		*bpp = 32;
	}

	// 16-bit BGRA / BGR
	else if (
		MATCH(wic, GUID_WICPixelFormat16bppBGR555) ||
		MATCH(wic, GUID_WICPixelFormat16bppBGRA5551)) 
	{ 
		*wicConvert = GUID_WICPixelFormat16bppBGRA5551;
		*dxgi = DXGI_FORMAT_B5G5R5A1_UNORM; 
		*bpp = 16;
	}
	else if (MATCH(wic, GUID_WICPixelFormat16bppBGR565)) 
	{ 
		*wicConvert = GUID_WICPixelFormat16bppBGR565;
		*dxgi = DXGI_FORMAT_B5G6R5_UNORM; 
		*bpp = 16;
	}

	// Single-channel
	else if (
		MATCH(wic, GUID_WICPixelFormat8bppGray) || 
		MATCH(wic, GUID_WICPixelFormatBlackWhite)) 
	{
		*wicConvert = GUID_WICPixelFormat8bppGray;
		*dxgi = DXGI_FORMAT_R8_UNORM; 
		*bpp = 8;
	}
	else if (MATCH(wic, GUID_WICPixelFormat16bppGray)) { *wicConvert = wic; *dxgi = DXGI_FORMAT_R16_UNORM; *bpp = 16; }
	else if (MATCH(wic, GUID_WICPixelFormat16bppGrayHalf)) { *wicConvert = wic; *dxgi = DXGI_FORMAT_R16_FLOAT; *bpp = 16; }
	else if (MATCH(wic, GUID_WICPixelFormat32bppGrayFloat)) { *wicConvert = wic; *dxgi = DXGI_FORMAT_R32_FLOAT; *bpp = 32; }

	// 64-bit & 128-bit RGBA
	else if (MATCH(wic, GUID_WICPixelFormat64bppRGBA)) { *wicConvert = wic; *dxgi = DXGI_FORMAT_R16G16B16A16_UNORM; *bpp = 64; }
	else if (MATCH(wic, GUID_WICPixelFormat64bppRGBAHalf)) { *wicConvert = wic; *dxgi = DXGI_FORMAT_R16G16B16A16_FLOAT; *bpp = 64; }
	else if (MATCH(wic, GUID_WICPixelFormat128bppRGBAFloat)) { *wicConvert = wic; *dxgi = DXGI_FORMAT_R32G32B32A32_FLOAT; *bpp = 128; }

#undef MATCH
}

// --------------------------------------------------------
// Loads a texture onto the GPU
// 
// device - D3D11 device object
// context - D3D11 context object
// imageFile - Image to load
// texture - Texture API object after creation (set to null if unneeded)
// srv - Shader Resource View API object (set to null if unneeded)
// generateMips - Should mip maps be generated?  Defaults to true
// --------------------------------------------------------
HRESULT LoadTexture(
	Microsoft::WRL::ComPtr<ID3D11Device> device, 
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
	const wchar_t* imageFile, 
	ID3D11Texture2D** texture, 
	ID3D11ShaderResourceView** srv,
	bool generateMips)
{
	// Potential results from calls below
	HRESULT hr = S_OK;

	// Factory to handle WIC object creation
	Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
	{
		hr = CoCreateInstance(
			CLSID_WICImagingFactory,	// The kind of object we want - the WIC factory
			0,							// Unnecessary here
			CLSCTX_INPROC_SERVER,		// Same process (program) in memory
			IID_PPV_ARGS(factory.GetAddressOf()));	// The factory itself

		// Exit on failure
		if (FAILED(hr))	return hr;
	}

	// Create a Windows Imagining Component decoder
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
	{
		hr = factory->CreateDecoderFromFilename(
			imageFile,
			0,
			GENERIC_READ,
			WICDecodeMetadataCacheOnDemand,
			decoder.GetAddressOf());

		// Exit on failure
		if (FAILED(hr))	return hr;
	}

	// Attempt to get the first "frame" of the image
	// - For regular images, this is the whole thing
	// - For animated images, this is the first of several
	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
	{
		hr = decoder->GetFrame(0, frame.GetAddressOf());
		if (FAILED(hr))	return hr;
	}

	// Grab the size of the image
	unsigned int imageWidth;
	unsigned int imageHeight;
	{
		hr = frame->GetSize(&imageWidth, &imageHeight);
		if (FAILED(hr))	return hr;
	}

	// Is the image too large? Max size is generally 16k
	unsigned int maxSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	if (imageWidth > maxSize || imageHeight > maxSize)
	{
		return E_FAIL;
	}

	// What color format does the image use?
	WICPixelFormatGUID wicFormat;
	{
		hr = frame->GetPixelFormat(&wicFormat);
		if (FAILED(hr))	return hr;
	}

	// We may need to convert to a compatible WIC format and DXGI format
	WICPixelFormatGUID desiredWicFormat;
	DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
	unsigned int bitsPerPixel = 0;
	GetFormatConversionDetails(wicFormat, &desiredWicFormat, &dxgiFormat, &bitsPerPixel);
	
	// Is a conversion necessary?
	bool convert = memcmp(&wicFormat, &desiredWicFormat, sizeof(GUID)) != 0;

	// Create the memory to hold the pixels
	unsigned int bytesPerRow = (imageWidth * bitsPerPixel) / 8;
	unsigned int totalBytes = bytesPerRow * imageHeight;
	unsigned char* pixelData = new unsigned char[totalBytes];

	// Do we need to convert?
	if (!convert)
	{
		// No conversion, just copy the data as is
		hr = frame->CopyPixels(0, bytesPerRow, totalBytes, pixelData);
		if (FAILED(hr))	return hr;
	}
	else
	{
		// Gotta convert first
		Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
		{
			hr = factory->CreateFormatConverter(converter.GetAddressOf());
			if (FAILED(hr))	return hr;
		}

		// Validate conversion
		BOOL canConvert = false;
		{
			hr = converter->CanConvert(wicFormat, desiredWicFormat, &canConvert);
			if (FAILED(hr))	return hr;
			if (!canConvert) return E_FAIL;
		}

		// Prepare converter
		hr = converter->Initialize(
			frame.Get(),
			desiredWicFormat,
			WICBitmapDitherTypeNone,
			0,
			0,
			WICBitmapPaletteTypeMedianCut);
		if (FAILED(hr))	return hr;

		// Now convert and copy the pixels
		hr = converter->CopyPixels(0, bytesPerRow, totalBytes, pixelData);
		if (FAILED(hr))	return hr;
	}

	// Should have data now!

	// Create the texture on the GPU
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = imageWidth;
	desc.Height = imageHeight;
	desc.MipLevels = generateMips ? 0 : 1;
	desc.ArraySize = 1;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags = 0;
	desc.Format = dxgiFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags = 0;

	if (generateMips)
	{
		desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	// Create the texture
	ID3D11Texture2D* localTexture;
	{
		// Not setting data yet, as if there are mips we have to
		// provide ALL mips worth of pixels.  Instead we'll just update
		// the top mip after the texture exists
		hr = device->CreateTexture2D(&desc, 0, &localTexture);
		if (FAILED(hr)) 
		{
			delete[] pixelData;
			return hr;
		}

		// Update the first subresource (the top mip)
		context->UpdateSubresource(localTexture, 0, 0, pixelData, bytesPerRow, totalBytes);

		// Delete the local pixel data now that it's copied to the GPU
		delete[] pixelData;
	}

	// Make the SRV 
	ID3D11ShaderResourceView* localSRV;
	{
		// Using default SRV (access to all subresources)
		hr = device->CreateShaderResourceView(localTexture, 0, &localSRV);
		if (FAILED(hr))
		{
			// Problem here, so release the texture too
			localTexture->Release();
			return hr;
		}
	}

	// Are we generating mips?
	if (generateMips)
	{
		context->GenerateMips(localSRV);
	}

	// Does the caller want the SRV?
	if (srv) *srv = localSRV;
	else localSRV->Release();

	// Does the caller want the texture ref?
	if (texture) *texture = localTexture;
	else localTexture->Release();

	// Success
	return S_OK;
}
