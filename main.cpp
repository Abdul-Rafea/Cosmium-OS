#include <windows.h>
#include <windowsx.h>

#include "direct-2D.h"

// Global transparency value (0 = invisible, 255 = solid)
BYTE g_alpha = 200;

// Function Prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Main Function
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"TransparentShellClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    // CRITICAL: WS_EX_LAYERED must be the first parameter in CreateWindowExW
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED,                      // Enables layered window attributes
        CLASS_NAME,
        L"Transparent Window - Use Up/Down Arrows to adjust alpha",
        WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 500,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, SW_MAXIMIZE);

    MSG msg = {};
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

// Function Definitions

bool IsPointInRect(float px, float py, const D2D1_RECT_F& r) {
    return (px >= r.left && px <= r.right && py >= r.top && py <= r.bottom);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            // Apply initial transparency when the window is created
            SetLayeredWindowAttributes(hwnd, 0, g_alpha, LWA_ALPHA);

            // Initialize Direct2D
            InitializeDirect2D(hwnd);

            // Load SVGs
            LoadSvgFile(L"x.svg", g_svgX.svg);
            g_svgX.onClick = [hwnd] {
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            };

            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);

            // Execute our Direct2D render pipeline (Draws SVGs)
            Render(hwnd);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_SIZE: {
            // Handle window resizing so the graphics buffer adjusts smoothly
            if (g_swapChain && wParam != SIZE_MINIMIZED) {
                g_d2dContext->SetTarget(nullptr); // Release target reference
                
                UINT width = LOWORD(lParam);
                UINT height = HIWORD(lParam);
                g_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

                // Re-bind the backbuffer to the Direct2D context
                Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
                g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

                D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
                    D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
                );
                Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
                g_d2dContext->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bp, &targetBitmap);
                g_d2dContext->SetTarget(targetBitmap.Get());
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            float mouseX = static_cast<float>(GET_X_LPARAM(lParam));
            float mouseY = static_cast<float>(GET_Y_LPARAM(lParam));

            bool hovered = IsPointInRect(mouseX, mouseY, g_svgX.bounds);
            if (hovered != g_svgX.isHovered) {
                g_svgX.isHovered = hovered;
                InvalidateRect(hwnd, nullptr, FALSE); // trigger redraw for hover state
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            float mouseX = static_cast<float>(GET_X_LPARAM(lParam));
            float mouseY = static_cast<float>(GET_Y_LPARAM(lParam));

            if (IsPointInRect(mouseX, mouseY, g_svgX.bounds) && g_svgX.onClick) {
                g_svgX.onClick();
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}