#include <windows.h>
#include <windowsx.h>

#include "direct-2D.h"
#include "terminal/terminal.h"

// Function Prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool IsPointInRect(float px, float py, const D2D1_RECT_F& r);
void ExecuteCommand(HWND hwnd, const std::wstring& cmd);
void SendCommandToSubprocess(const std::wstring& cmd);
void StopSubprocessTerminal();

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
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Applying transparency
            SetLayeredWindowAttributes(hwnd, 0, 200, LWA_ALPHA);

            // Initializing direct2D
            InitializeDirect2D(hwnd);

            // Loading svgs
            LoadSvgFile(L"battery-unplugged.svg", g_svgBatteryUnplugged);
            LoadSvgFile(L"battery-plugged.svg", g_svgBatteryPlugged);

            // Checking initial power state on boot
            SYSTEM_POWER_STATUS status;
            if (GetSystemPowerStatus(&status)) {
                g_batteryPercent = (status.BatteryLifePercent == 255) ? 0 : status.BatteryLifePercent;
                g_isPluggedIn = (status.ACLineStatus == 1);
            }

            // Intializing terminal
            initializeTerminal(hwnd);
            
            // Timer
            SetTimer(hwnd, 1, 5000, nullptr);

            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);

            // Executing direct2D render pipelines
            render(hwnd);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_TIMER: {
            if (wParam == 1) {
                SYSTEM_POWER_STATUS status;
                if (GetSystemPowerStatus(&status)) {
                    int currentPercent = (status.BatteryLifePercent == 255) ? 0 : status.BatteryLifePercent;
                    bool currentPluggedIn = (status.ACLineStatus == 1);

                    // If percentage or power source changed, update globals and redraw
                    if (currentPercent != g_batteryPercent || currentPluggedIn != g_isPluggedIn) {
                        g_batteryPercent = currentPercent;
                        g_isPluggedIn = currentPluggedIn;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
            }
            return 0;
        }

        case WM_POWERBROADCAST: {
            // Check if the power status has changed
            if (wParam == PBT_APMPOWERSTATUSCHANGE) {
                SYSTEM_POWER_STATUS status;
                if (GetSystemPowerStatus(&status)) {
                    g_batteryPercent = (status.BatteryLifePercent == 255) ? 0 : status.BatteryLifePercent;
                    g_isPluggedIn = (status.ACLineStatus == 1);
                    InvalidateRect(hwnd, nullptr, FALSE); // Trigger redraw
                }
            }
            return TRUE;
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

        case WM_CHAR: {
            wchar_t ch = static_cast<wchar_t>(wParam);

            if (ch == L'\r') { // Enter key
                SendCommandToSubprocess(g_terminal.currentInput);
                g_terminal.currentInput.clear();
            } else if (ch == L'\b') { // Backspace
                if (!g_terminal.currentInput.empty()) {
                    g_terminal.currentInput.pop_back();
                }
            } else if (ch >= 32) {
                g_terminal.currentInput += ch;
            }

            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            // Scroll down to reveal newer lines; scroll up to return to the beginning.
            g_terminal.scrollOffset += (delta > 0) ? -30.0f : 30.0f;

            // Clamp scroll so you don't scroll past the bottom or past the top
            float boxHeight = g_terminal.bounds.bottom - g_terminal.bounds.top;
            std::size_t historySize;
            {
                std::lock_guard<std::mutex> lock(g_terminal.historyMutex);
                historySize = g_terminal.history.size();
            }
            float totalContentHeight = (historySize + 1) * g_terminal.lineHeight;
            float maxScroll = (totalContentHeight > boxHeight) ? (totalContentHeight - boxHeight + 20.0f) : 0.0f;

            if (g_terminal.scrollOffset < 0.0f) g_terminal.scrollOffset = 0.0f;
            if (g_terminal.scrollOffset > maxScroll) g_terminal.scrollOffset = maxScroll;

            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_APP + 1: {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        case WM_MOUSEMOVE: {
            float mouseX = static_cast<float>(GET_X_LPARAM(lParam));
            float mouseY = static_cast<float>(GET_Y_LPARAM(lParam));

            return 0;
        }

        case WM_LBUTTONDOWN: {
            float mouseX = static_cast<float>(GET_X_LPARAM(lParam));
            float mouseY = static_cast<float>(GET_Y_LPARAM(lParam));

            return 0;
        }

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            stopTerminal();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}

bool IsPointInRect(float px, float py, const D2D1_RECT_F& r) {
    return (px >= r.left && px <= r.right && py >= r.top && py <= r.bottom);
}

void ExecuteCommand(HWND hwnd, const std::wstring& cmd) {
    // Save the typed command to history
    g_terminal.history.push_back(L"> " + cmd);

    if (cmd == L"help") {
        g_terminal.history.push_back(L"Available commands: help, clear, battery, exit");
    } else if (cmd == L"clear") {
        g_terminal.history.clear();
    } else if (cmd == L"battery") {
        g_terminal.history.push_back(L"Battery: " + std::to_wstring(g_batteryPercent) + L"% " + (g_isPluggedIn ? L"(Plugged In)" : L"(On Battery)"));
    } else if (cmd == L"exit") {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    } else if (!cmd.empty()) {
        g_terminal.history.push_back(L"Unknown command: " + cmd);
    }

    // Auto-scroll back to the bottom whenever a new command runs
    g_terminal.scrollOffset = 0.0f;
}

void SendCommandToSubprocess(const std::wstring& cmd) {
    // Echo command locally in history box
    {
        std::lock_guard<std::mutex> lock(g_terminal.historyMutex);
        g_terminal.history.push_back(L"> " + cmd);
    }

    // Append newline so cmd.exe executes it
    std::wstring fullCmd = cmd + L"\r\n";
    int narrowLength = WideCharToMultiByte(CP_ACP, 0, fullCmd.c_str(), static_cast<int>(fullCmd.size()),
        nullptr, 0, nullptr, nullptr);
    std::string narrowCmd(static_cast<size_t>(narrowLength), '\0');
    WideCharToMultiByte(CP_ACP, 0, fullCmd.c_str(), static_cast<int>(fullCmd.size()),
        narrowCmd.data(), narrowLength, nullptr, nullptr);

    DWORD written;
    if (!WriteFile(g_terminal.hChildStdinWrite, narrowCmd.data(), static_cast<DWORD>(narrowCmd.size()), &written, nullptr)) {
        std::lock_guard<std::mutex> lock(g_terminal.historyMutex);
        g_terminal.history.push_back(L"Failed to send command to cmd.exe.");
    }
    
    g_terminal.scrollOffset = 0.0f;
}
