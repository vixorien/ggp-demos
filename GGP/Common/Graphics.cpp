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
