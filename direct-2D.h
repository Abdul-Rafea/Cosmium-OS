#pragma once

#include <windows.h>
#include <dxgi1_2.h>
#include <d2d1_3.h>
#include <wrl/client.h>
#include <dwrite.h>
#include <d3d11.h>
#include <shlwapi.h>
#include <string>
#include <functional>
#include <wincodec.h>

#include "util/util.h"
#include "terminal/terminal.h"

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "windowscodecs.lib")

// Structs
struct Png {
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> png; 
    D2D1_RECT_F rect;
    float opacity = 1.0f;
};

struct Text {
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    D2D1_POINT_2F origin;
    std::wstring text;
    float fontSize;
};

// Global Direct2D / DXGI Interfaces
inline Microsoft::WRL::ComPtr<ID2D1Factory6> g_d2dFactory;
inline Microsoft::WRL::ComPtr<ID2D1Device5> g_d2dDevice;
inline Microsoft::WRL::ComPtr<ID2D1DeviceContext5> g_d2dContext;
inline Microsoft::WRL::ComPtr<IDXGISwapChain1> g_swapChain;
inline Microsoft::WRL::ComPtr<IDWriteFactory> g_dWriteFactory;
inline Microsoft::WRL::ComPtr<IDWriteTextFormat> g_textFormat;
inline Microsoft::WRL::ComPtr<IWICImagingFactory> g_wicFactory;

// Global Brushes
inline Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> g_whiteBrush;

// Global Direct 2D Componnets
inline Microsoft::WRL::ComPtr<ID2D1SvgDocument> g_svgBatteryUnplugged;
inline Microsoft::WRL::ComPtr<ID2D1SvgDocument> g_svgBatteryPlugged;
inline bool g_isPluggedIn = false;
inline int g_batteryPercent = 100;

inline Png g_logo;
inline Text g_loadingHeading;

// Functions
HRESULT initializeDirect2D(HWND hwnd);
HRESULT loadSvgFile(const wchar_t* filename, Microsoft::WRL::ComPtr<ID2D1SvgDocument>& outSvgDoc);
HRESULT loadPngFile(const wchar_t* filename, Microsoft::WRL::ComPtr<ID2D1Bitmap1>& outBitmap);
HRESULT configureText(
    Text& component,
    const std::wstring& string,
    float fontSize,
    D2D1_POINT_2F origin,
    float maxWidth,
    float maxHeight,
    std::wstring color
);
void renderPng();
void renderText(Text& text);
void render(HWND hwnd);