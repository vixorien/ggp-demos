#include "Graphics.h"
#include <dxgi1_6.h>

// Tell the drivers to use high-performance GPU in multi-GPU systems (like laptops)
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001; // NVIDIA
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; // AMD
}

namespace Graphics
{
	// Annonymous namespace to hold variables
	// only accessible in this file
	namespace
	{
		bool apiInitialized = false;
		bool supportsTearing = false;
		bool vsyncDesired = false;
		BOOL isFullscreen = false;

		D3D_FEATURE_LEVEL featureLevel;

		Microsoft::WRL::ComPtr<ID3D11InfoQueue> InfoQueue;

		// Constant buffer management
		unsigned int cbHeapSizeInBytes = 0;
		unsigned int cbHeapOffsetInBytes = 0;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context1;
	}
}

// Getters
bool Graphics::VsyncState() { return vsyncDesired || !supportsTearing || isFullscreen; }
std::wstring Graphics::APIName() 
{ 
	switch (featureLevel)
	{
	case D3D_FEATURE_LEVEL_10_0: return L"D3D10";
	case D3D_FEATURE_LEVEL_10_1: return L"D3D10.1";
	case D3D_FEATURE_LEVEL_11_0: return L"D3D11";
	case D3D_FEATURE_LEVEL_11_1: return L"D3D11.1";
	default: return L"Unknown";
	}
}

// --------------------------------------------------------
// Initializes the Graphics API, which requires window details.
// 
// windowWidth     - Width of the window (and our viewport)
// windowHeight    - Height of the window (and our viewport)
// windowHandle    - OS-level handle of the window
// vsyncIfPossible - Sync to the monitor's refresh rate if available?
// --------------------------------------------------------
HRESULT Graphics::Initialize(unsigned int windowWidth, unsigned int windowHeight, HWND windowHandle, bool vsyncIfPossible)
{
	// Only initialize once
	if (apiInitialized)
		return E_FAIL;

	// Save desired vsync state, though it may be stuck "on" if
	// the device doesn't support screen tearing
	vsyncDesired = vsyncIfPossible;

	// Determine if screen tearing ("vsync off") is available
	// - This is necessary due to variable refresh rate displays
	Microsoft::WRL::ComPtr<IDXGIFactory5> factory;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
	{
		// Check for this specific feature (must use BOOL typedef here!)
		BOOL tearingSupported = false;
		HRESULT featureCheck = factory->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&tearingSupported,
			sizeof(tearingSupported));

		// Final determination of support
		supportsTearing = SUCCEEDED(featureCheck) && tearingSupported;
	}

	// This will hold options for DirectX initialization
	unsigned int deviceFlags = 0;

#if defined(DEBUG) || defined(_DEBUG)
	// If we're in debug mode in visual studio, we also
	// want to make a "Debug DirectX Device" to see some
	// errors and warnings in Visual Studio's output window
	// when things go wrong!
	deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	// Create a description of how our swap
	// chain should work
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount		= 2;
	swapDesc.BufferDesc.Width	= windowWidth;
	swapDesc.BufferDesc.Height	= windowHeight;
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapDesc.BufferDesc.Format	= DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapDesc.BufferUsage		= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.Flags				= supportsTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	swapDesc.OutputWindow		= windowHandle;
	swapDesc.SampleDesc.Count	= 1;
	swapDesc.SampleDesc.Quality = 0;
	swapDesc.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapDesc.Windowed			= true;

	// Result variable for below function calls
	HRESULT hr = S_OK;

	// Attempt to initialize DirectX
	hr = D3D11CreateDeviceAndSwapChain(
		0,							// Video adapter (physical GPU) to use, or null for default
		D3D_DRIVER_TYPE_HARDWARE,	// We want to use the hardware (GPU)
		0,							// Used when doing software rendering
		deviceFlags,				// Any special options
		0,							// Optional array of possible verisons we want as fallbacks
		0,							// The number of fallbacks in the above param
		D3D11_SDK_VERSION,			// Current version of the SDK
		&swapDesc,					// Address of swap chain options
		SwapChain.GetAddressOf(),	// Pointer to our Swap Chain pointer
		Device.GetAddressOf(),		// Pointer to our Device pointer
		&featureLevel,				// Retrieve exact API feature level in use
		Context.GetAddressOf());	// Pointer to our Device Context pointer
	if (FAILED(hr)) return hr;

	// We're set up
	apiInitialized = true;

	// Call ResizeBuffers(), which will also set up the 
	// render target view and depth stencil view for the
	// various buffers we need for rendering. This call 
	// will also set the appropriate viewport.
	ResizeBuffers(windowWidth, windowHeight);

#if defined(DEBUG) || defined(_DEBUG)
	// If we're in debug mode, set up the info queue to
	// get debug messages we can print to our console
	Microsoft::WRL::ComPtr<ID3D11Debug> debug;
	Device->QueryInterface(IID_PPV_ARGS(debug.GetAddressOf()));
	debug->QueryInterface(IID_PPV_ARGS(InfoQueue.GetAddressOf()));
#endif

	// Grab the Direct3D 11.1 version of the context for later
	Context->QueryInterface<ID3D11DeviceContext1>(context1.GetAddressOf());

	return S_OK;
}

// --------------------------------------------------------
// Called at the end of the program to clean up any
// graphics API specific memory. 
// 
// This exists for completeness since D3D objects generally
// use ComPtrs, which get cleaned up automatically.  Other
// APIs might need more explicit clean up.
// --------------------------------------------------------
void Graphics::ShutDown()
{
}


// --------------------------------------------------------
// When the window is resized, the underlying 
// buffers (textures) must also be resized to match.
//
// If we don't do this, the window size and our rendering
// resolution won't match up.  This can result in odd
// stretching/skewing.
// 
// width  - New width of the window (and our viewport)
// height - New height of the window (and our viewport)
// --------------------------------------------------------
void Graphics::ResizeBuffers(unsigned int width, unsigned int height)
{
	// Ensure graphics API is initialized
	if (!apiInitialized)
		return;

	BackBufferRTV.Reset();
	DepthBufferDSV.Reset();

	// Resize the swap chain buffers
	SwapChain->ResizeBuffers(
		2, 
		width, 
		height, 
		DXGI_FORMAT_R8G8B8A8_UNORM, 
		supportsTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

	// Grab the references to the first buffer
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBufferTexture;
	SwapChain->GetBuffer(
		0,
		__uuidof(ID3D11Texture2D),
		(void**)backBufferTexture.GetAddressOf());

	// Now that we have the texture, create a render target view
	// for the back buffer so we can render into it.
	Device->CreateRenderTargetView(
		backBufferTexture.Get(),
		0,
		BackBufferRTV.GetAddressOf());

	// Set up the description of the texture to use for the depth buffer
	D3D11_TEXTURE2D_DESC depthStencilDesc = {};
	depthStencilDesc.Width = width;
	depthStencilDesc.Height = height;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;

	// Create the depth buffer and its view, then 
	// release our reference to the texture
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthBufferTexture;
	Device->CreateTexture2D(&depthStencilDesc, 0, &depthBufferTexture);
	Device->CreateDepthStencilView(
		depthBufferTexture.Get(),
		0,
		DepthBufferDSV.GetAddressOf()); 

	// Bind the views to the pipeline, so rendering properly 
	// uses their underlying textures
	Context->OMSetRenderTargets(
		1,
		BackBufferRTV.GetAddressOf(), // This requires a pointer to a pointer (an array of pointers), so we get the address of the pointer
		DepthBufferDSV.Get());

	// Lastly, set up a viewport so we render into
	// to correct portion of the window
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)width;
	viewport.Height = (float)height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	Context->RSSetViewports(1, &viewport);

	// Are we in a fullscreen state?
	SwapChain->GetFullscreenState(&isFullscreen, 0);
}


// --------------------------------------------------------
// Creates (or recreates) the large constant buffer we
// can use as a heap of smaller constant buffers.
// 
// sizeInBytes - The size of the buffer in bytes.  Note that
//               the size will be aligned to the next highest
//               multiple of 256 to match binding requirements
// --------------------------------------------------------
void Graphics::ResizeConstantBufferHeap(unsigned int sizeInBytes)
{
	// Ensure graphics API is initialized
	if (!apiInitialized)
		return;

	// Resets the ComPtr, releasing any existing references
	ConstantBufferHeap.Reset();

	// Set up basic size tracking details
	cbHeapOffsetInBytes = 0;
	cbHeapSizeInBytes = (sizeInBytes + 255) / 256 * 256;

	// Create the actual buffer
	D3D11_BUFFER_DESC cbDesc{};
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.ByteWidth = cbHeapSizeInBytes;
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;
	Device->CreateBuffer(&cbDesc, 0, ConstantBufferHeap.GetAddressOf());
}


// --------------------------------------------------------
// Copies the given data into the next "unused" spot in
// the constant buffer and then binds it to the specied
// location in the pipeline.
// 
// data - The data to copy to the GPU
// dataSizeInBytes - The byte size of the data to copy
// shaderType - The shader stage for binding
// registerSlot - The slot for binding
// --------------------------------------------------------
void Graphics::FillAndSetNextConstantBuffer(
	void* data,
	unsigned int dataSizeInBytes,
	D3D11_SHADER_TYPE shaderType,
	unsigned int registerSlot)
{
	// How much space will we actually need?  Each chunk must be
	// a multiple of 256 bytes.  Performating a basic alignment here.
	unsigned int reservationSize = (dataSizeInBytes + 255) / 256 * 256;

	// Does this fit in the remaining space?  If not, loop back to
	// the beginning of the ring buffer
	if (cbHeapOffsetInBytes + reservationSize >= cbHeapSizeInBytes)
		cbHeapOffsetInBytes = 0;

	// Map the buffer, promising not to overwrite any data currently
	// in use by a call in flight.  This is accomplished with the
	// MAP_WRITE_NO_OVERWRITE flag below.  This allows us to quickly
	// update portions of the resource that aren't in use.
	D3D11_MAPPED_SUBRESOURCE map{};
	Context->Map(
		ConstantBufferHeap.Get(),
		0,
		D3D11_MAP_WRITE_NO_OVERWRITE, // Must ensure we're not touching memory currently in use!!!
		0,
		&map);

	// Write into the proper portion of the buffer
	void* uploadAddress = reinterpret_cast<void*>((UINT64)map.pData + cbHeapOffsetInBytes);
	memcpy(uploadAddress, data, dataSizeInBytes);

	// Unmap to release this portion of the buffer
	Context->Unmap(ConstantBufferHeap.Get(), 0);

	// Calculate the offset and size as measured in 16-byte constants
	unsigned int firstConstant = cbHeapOffsetInBytes / 16;
	unsigned int numConstants = reservationSize / 16;

	// Bind the buffer to the proper pipeline stage
	switch (shaderType)
	{
	case D3D11_VERTEX_SHADER:
		context1->VSSetConstantBuffers1(
			registerSlot,
			1,
			ConstantBufferHeap.GetAddressOf(),
			&firstConstant,
			&numConstants);
		break;

	case D3D11_PIXEL_SHADER:
		context1->PSSetConstantBuffers1(
			registerSlot,
			1,
			ConstantBufferHeap.GetAddressOf(),
			&firstConstant,
			&numConstants);
		break;
	}

	// Offset for the next call
	cbHeapOffsetInBytes += reservationSize;
}


// --------------------------------------------------------
// Prints graphics debug messages waiting in the queue
// --------------------------------------------------------
void Graphics::PrintDebugMessages()
{
	// Do we actually have an info queue (usually in debug mode)
	if (!InfoQueue)
		return;

	// Any messages?
	UINT64 messageCount = InfoQueue->GetNumStoredMessages();
	if (messageCount == 0)
		return;

	// Loop and print messages
	for (UINT64 i = 0; i < messageCount; i++)
	{
		// Get the size so we can reserve space
		size_t messageSize = 0;
		InfoQueue->GetMessage(i, 0, &messageSize);

		// Reserve space for this message
		D3D11_MESSAGE* message = (D3D11_MESSAGE*)malloc(messageSize);
		InfoQueue->GetMessage(i, message, &messageSize);
		
		// Print and clean up memory
		if (message)
		{
			// Color code based on severity
			switch (message->Severity)
			{
			case D3D11_MESSAGE_SEVERITY_CORRUPTION:
			case D3D11_MESSAGE_SEVERITY_ERROR:
				printf("\x1B[91m"); break; // RED

			case D3D11_MESSAGE_SEVERITY_WARNING:
				printf("\x1B[93m"); break; // YELLOW

			case D3D11_MESSAGE_SEVERITY_INFO:
			case D3D11_MESSAGE_SEVERITY_MESSAGE:
				printf("\x1B[96m"); break; // CYAN
			}

			printf("%s\n\n", message->pDescription);
			free(message);

			// Reset color
			printf("\x1B[0m");
		}
	}

	// Clear any messages we've printed
	InfoQueue->ClearStoredMessages();
}
