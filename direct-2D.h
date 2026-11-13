#pragma once

#include <windows.h>
#include <dxgi1_2.h>
#include <d2d1_3.h>
#include <wrl/client.h>
#include <functional>

// Structs
struct buttonSvg {
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

// SVG Variables
inline buttonSvg g_svgX;

// Functions
HRESULT InitializeDirect2D(HWND hwnd);
HRESULT LoadSvgFile(const wchar_t* filename, Microsoft::WRL::ComPtr<ID2D1SvgDocument>& outSvgDoc);
void Render(HWND hwnd);