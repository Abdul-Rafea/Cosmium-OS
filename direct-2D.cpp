#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_3.h>
#include <shlwapi.h>
#include <wrl/client.h> // For Microsoft::WRL::ComPtr
#include <string>

#include "direct-2D.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "shlwapi.lib")

using Microsoft::WRL::ComPtr;

HRESULT InitializeDirect2D(HWND hwnd) {
    // 1. Create Direct3D 11 Device
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    ComPtr<ID3D11Device> d3dDevice;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
        nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, nullptr, nullptr);
    if (FAILED(hr)) return hr;

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) return hr;

    // 2. Create Direct2D Factory and Device Context 5
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory6), &g_d2dFactory);
    if (FAILED(hr)) return hr;

    hr = g_d2dFactory->CreateDevice(dxgiDevice.Get(), &g_d2dDevice);
    if (FAILED(hr)) return hr;

    hr = g_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g_d2dContext);
    if (FAILED(hr)) return hr;

    // 3. Create SwapChain for the HWND
    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) return hr;

    ComPtr<IDXGIFactory2> dxgiFactory;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr)) return hr;

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.SampleDesc.Count = 1;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    hr = dxgiFactory->CreateSwapChainForHwnd(d3dDevice.Get(), hwnd, &scDesc, nullptr, nullptr, &g_swapChain);
    if (FAILED(hr)) return hr;

    // 4. Bind SwapChain Backbuffer to Direct2D Render Target
    ComPtr<IDXGISurface> backBuffer;
    hr = g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return hr;

    D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    ComPtr<ID2D1Bitmap1> targetBitmap;
    hr = g_d2dContext->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bp, &targetBitmap);
    if (FAILED(hr)) return hr;

    g_d2dContext->SetTarget(targetBitmap.Get());

    return S_OK; // Everything initialized cleanly
}

HRESULT LoadSvgFile(const wchar_t* filename, Microsoft::WRL::ComPtr<ID2D1SvgDocument>& outSvgDoc) {
    if (!g_d2dContext) return E_FAIL;

    Microsoft::WRL::ComPtr<IStream> svgStream;
    HRESULT hr = SHCreateStreamOnFileW(filename, STGM_READ, &svgStream);
    if (SUCCEEDED(hr)) {
        D2D1_SIZE_F viewportSize = D2D1::SizeF(64.0f, 64.0f);
        g_d2dContext->CreateSvgDocument(svgStream.Get(), viewportSize, &outSvgDoc);
    }
    return hr;
}

void Render(HWND hwnd) {
    if (!g_d2dContext) return;

    // Get current window dimensions
    RECT window;
    GetClientRect(hwnd, &window);

    // Calculate Coordinates and Sizes
    float windowWidth = static_cast<float>(window.right - window.left);
    float nativeSvgSize = 24.0f;
    

    g_d2dContext->BeginDraw();

    if (g_svgX.svg) {
        float desiredSvgSize = 100.0f;
        float scale = desiredSvgSize / nativeSvgSize;
        float padding  = 0.0f;
        float x = windowWidth - desiredSvgSize - padding;
        float y = padding;

        g_svgX.bounds = D2D1::RectF(x, y, x + desiredSvgSize, y + desiredSvgSize);
        if (g_svgX.isHovered) {
            Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> hoverBrush;
            g_d2dContext->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), &hoverBrush);
            g_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(g_svgX.bounds, 6.0f, 6.0f), hoverBrush.Get());
        }
        
        D2D1::Matrix3x2F transform = D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Translation(x, y);
        g_d2dContext->SetTransform(transform); // move the drawing origin to the top-right corner
        g_d2dContext->DrawSvgDocument(g_svgX.svg.Get());
        g_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity()); // reset drawing origin back to (0,0)
    }

    g_d2dContext->EndDraw();
    g_swapChain->Present(1, 0);
}