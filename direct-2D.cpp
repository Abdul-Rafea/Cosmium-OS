#include "direct-2D.h"

// Functions
HRESULT initializeDirect2D(HWND hwnd) {
    // Create Direct3D 11 Device
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
        nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, nullptr, nullptr);
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) return hr;

    // Create Direct2D Factory and Device Context 5
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory6), &g_d2dFactory);
    if (FAILED(hr)) return hr;

    hr = g_d2dFactory->CreateDevice(dxgiDevice.Get(), &g_d2dDevice);
    if (FAILED(hr)) return hr;

    hr = g_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &g_d2dContext);
    if (FAILED(hr)) return hr;

    // Create SwapChain for the HWND
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
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

    // Bind SwapChain Backbuffer to Direct2D Render Target
    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    hr = g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return hr;

    D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    hr = g_d2dContext->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bp, &targetBitmap);
    if (FAILED(hr)) return hr;

    g_d2dContext->SetTarget(targetBitmap.Get());

    // Create DirectWrite Factory and Text Format
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &g_dWriteFactory);
    if (FAILED(hr)) return hr;

    hr = g_dWriteFactory->CreateTextFormat(
        L"Consolas",                  // Font family name (or any custom font installed on Windows)
        nullptr,                      // Font collection (nullptr = system fonts)
        DWRITE_FONT_WEIGHT_SEMI_BOLD,  // Weight
        DWRITE_FONT_STYLE_NORMAL,     // Style
        DWRITE_FONT_STRETCH_NORMAL,   // Stretch
        14.0f,                        // Font size (in DIPs/pixels)
        L"en-US",                     // Locale
        &g_textFormat
    );
    if (FAILED(hr)) return hr;

    // Initializing WIC factory
    hr = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&g_wicFactory)
    );
    if (FAILED(hr)) return hr;

    // Creating brushes
    hr = g_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &g_whiteBrush);
    if (FAILED(hr)) return hr;

    return S_OK;
}

HRESULT loadSvgFile(const wchar_t* filename, Microsoft::WRL::ComPtr<ID2D1SvgDocument>& outSvgDoc) {
    if (!g_d2dContext) return E_FAIL;

    Microsoft::WRL::ComPtr<IStream> svgStream;
    HRESULT hr = SHCreateStreamOnFileW(filename, STGM_READ, &svgStream);
    if (SUCCEEDED(hr)) {
        D2D1_SIZE_F viewportSize = D2D1::SizeF(24.0f, 24.0f);
        g_d2dContext->CreateSvgDocument(svgStream.Get(), viewportSize, &outSvgDoc);
    }
    return hr;
}

HRESULT loadPngFile(const wchar_t* filename, Microsoft::WRL::ComPtr<ID2D1Bitmap1>& outBitmap) {
    if (!g_wicFactory || !g_d2dContext) return E_FAIL;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = g_wicFactory->CreateDecoderFromFilename(
        filename,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        &decoder
    );
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return hr;

    // Convert to Premultiplied BGRA so Direct2D blends alpha cleanly
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = g_wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return hr;

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0f,
        WICBitmapPaletteTypeMedianCut
    );
    if (FAILED(hr)) return hr;

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    hr = g_d2dContext->CreateBitmapFromWicBitmap(converter.Get(), &props, &outBitmap);
    if (FAILED(hr)) return hr;

    return S_OK;
}

HRESULT configureText(
    Text& component,
    const std::wstring& string,
    float fontSize,
    D2D1_POINT_2F origin,
    float maxWidth,
    float maxHeight,
    std::wstring color
) {
    if (!g_d2dContext || !g_dWriteFactory || !g_textFormat) return E_FAIL;

    component.text = string;
    component.fontSize = fontSize;
    component.origin = origin;

    if(color == L"white") {
        component.brush = g_whiteBrush;
    }

    HRESULT hr = g_dWriteFactory->CreateTextLayout(
        component.text.c_str(),
        static_cast<UINT32>(component.text.length()),
        g_textFormat.Get(),
        maxWidth,
        maxHeight,
        &component.layout
    );
    if (FAILED(hr)) return hr;

    if (!component.text.empty()) {
        DWRITE_TEXT_RANGE fullRange = { 0, static_cast<UINT32>(component.text.length()) };
        hr = component.layout->SetFontSize(component.fontSize, fullRange);
        if (FAILED(hr)) return hr;
    }

    return S_OK;
}

void renderPng() {
    if (g_logo.png) {
        g_d2dContext->DrawBitmap(
            g_logo.png.Get(),
            &g_logo.rect,
            g_logo.opacity, // opacity
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
        );
    }
}

void renderText(Text& component) {
    if(component.layout) {
        g_d2dContext->DrawTextLayout(
            component.origin,
            component.layout.Get(),
            component.brush.Get()
        );
    }
}