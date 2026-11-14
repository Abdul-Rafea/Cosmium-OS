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

#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "shlwapi.lib")

// Structs
struct SvgLayout {
    float desiredSize = 0.0f;
    float scale = 0.0f;
    float x = 0.90f;
    float y = 0.0f;
};

struct ButtonSvg {
    Microsoft::WRL::ComPtr<ID2D1SvgDocument> svg;
    D2D1_RECT_F bounds = {};   // left, top, right, bottom
    bool isHovered = false;
    std::function<void()> onClick;
};

// Global Direct2D / DXGI Interfaces
inline Microsoft::WRL::ComPtr<ID2D1Factory6> g_d2dFactory;
inline Microsoft::WRL::ComPtr<ID2D1Device5> g_d2dDevice;
inline Microsoft::WRL::ComPtr<ID2D1DeviceContext5> g_d2dContext;
inline Microsoft::WRL::ComPtr<IDXGISwapChain1> g_swapChain;

inline Microsoft::WRL::ComPtr<IDWriteFactory> g_dWriteFactory;
inline Microsoft::WRL::ComPtr<IDWriteTextFormat> g_textFormat;

// Global SVG Variables
inline ButtonSvg g_svgX;
inline Microsoft::WRL::ComPtr<ID2D1SvgDocument> g_svgBatteryUnplugged;
inline Microsoft::WRL::ComPtr<ID2D1SvgDocument> g_svgBatteryPlugged;
inline bool g_isPluggedIn = false;
inline int g_batteryPercent = 100;

// Functions
HRESULT InitializeDirect2D(HWND hwnd);
HRESULT LoadSvgFile(const wchar_t* filename, Microsoft::WRL::ComPtr<ID2D1SvgDocument>& outSvgDoc);
void Render(HWND hwnd);