#include "direct-2D.h"

using Microsoft::WRL::ComPtr;

// Helper Functions
SvgLayout calcSvgLayout(float desiredSize, float windowWidth, float paddingX, float paddingY, std::wstring position) {
    SvgLayout svg;
    svg.desiredSize = desiredSize;
    svg.scale = desiredSize / 24;

    if(position == L"TOP_RIGHT") {
        svg.x = windowWidth - desiredSize - paddingX;
        svg.y = paddingY;
    }
    else if (position == L"TOP_LEFT") {
        svg.x = paddingX;
        svg.y = paddingY;
    }

    return svg;
}

void transformSpaceForSvg(ID2D1SvgDocument* svgDoc, const SvgLayout& layout) {
    if (!svgDoc || !g_d2dContext) return;

    D2D1::Matrix3x2F transform = 
        D2D1::Matrix3x2F::Scale(layout.scale, layout.scale) * 
        D2D1::Matrix3x2F::Translation(layout.x, layout.y);

    g_d2dContext->SetTransform(transform);
    g_d2dContext->DrawSvgDocument(svgDoc);
    g_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
}

// Functions
HRESULT InitializeDirect2D(HWND hwnd) {
    // Create Direct3D 11 Device
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    ComPtr<ID3D11Device> d3dDevice;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
        nullptr, 0, D3D11_SDK_VERSION, &d3dDevice, nullptr, nullptr);
    if (FAILED(hr)) return hr;

    ComPtr<IDXGIDevice> dxgiDevice;
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

    // Bind SwapChain Backbuffer to Direct2D Render Target
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

    // Create DirectWrite Factory and Text Format
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &g_dWriteFactory);
    if (FAILED(hr)) return hr;

    hr = g_dWriteFactory->CreateTextFormat(
        L"Segoe UI",                  // Font family name (or any custom font installed on Windows)
        nullptr,                      // Font collection (nullptr = system fonts)
        DWRITE_FONT_WEIGHT_SEMI_BOLD,  // Weight
        DWRITE_FONT_STYLE_NORMAL,     // Style
        DWRITE_FONT_STRETCH_NORMAL,   // Stretch
        14.0f,                        // Font size (in DIPs/pixels)
        L"en-US",                     // Locale
        &g_textFormat
    );
    if (FAILED(hr)) return hr;

    return S_OK;
}

HRESULT LoadSvgFile(const wchar_t* filename, Microsoft::WRL::ComPtr<ID2D1SvgDocument>& outSvgDoc) {
    if (!g_d2dContext) return E_FAIL;

    Microsoft::WRL::ComPtr<IStream> svgStream;
    HRESULT hr = SHCreateStreamOnFileW(filename, STGM_READ, &svgStream);
    if (SUCCEEDED(hr)) {
        D2D1_SIZE_F viewportSize = D2D1::SizeF(24.0f, 24.0f);
        g_d2dContext->CreateSvgDocument(svgStream.Get(), viewportSize, &outSvgDoc);
    }
    return hr;
}

void render(HWND hwnd) {
    // Checking direct 2D context
    if (!g_d2dContext) return;

    // Geting current window dimensions
    float windowWidth = getWindowWidth(hwnd);
    float windowHeight = getWindowHeight(hwnd);

    // Start drawing and clear drawing frame
    g_d2dContext->BeginDraw();
    g_d2dContext->Clear(D2D1::ColorF(0.08f, 0.09f, 0.11f, 1.0f));

    // Svgs
    ID2D1SvgDocument* batterySvg = g_isPluggedIn ? g_svgBatteryPlugged.Get() : g_svgBatteryUnplugged.Get();

    if (batterySvg) {
        SvgLayout svg = calcSvgLayout(50.0f, windowWidth, 10.0f, 0.0f, L"TOP_LEFT");
        transformSpaceForSvg(batterySvg, svg);

        // Battery text bpunds
        float textX = svg.x + svg.desiredSize + 5.0f;
        float textY = svg.y + 8.0f; // Vertically centered
        
        // D2D1_RECT_F textRect = D2D1::RectF(textX, textY, textX + 100.0f, textY + 24.0f);

        // Createing brush for the text
        Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush;
        g_d2dContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &textBrush);

        std::wstring batteryText = std::to_wstring(g_batteryPercent) + L"%";

        // Creating a layout
        Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;
        g_dWriteFactory->CreateTextLayout(
            batteryText.c_str(),
            static_cast<UINT32>(batteryText.length()),
            g_textFormat.Get(),
            200.0f, // Max layout width
            50.0f,  // Max layout height
            &textLayout
        );

        // Targeting the exact range and overriding the font size dynamically
        DWRITE_TEXT_RANGE range = { 0, static_cast<UINT32>(batteryText.length()) };
        textLayout->SetFontSize(24.0f, range); // <--- Changes font size for this layout instantly

        // 3. Drawing the layout
        g_d2dContext->DrawTextLayout(
            D2D1::Point2F(textX, textY),
            textLayout.Get(),
            textBrush.Get()
        );
    }

    // Terminal
    renderTerminal(hwnd, windowWidth, windowHeight);   

    // TO BE REVIEWED
    // Ending drawing
    g_d2dContext->EndDraw();
    g_swapChain->Present(1, 0);
}