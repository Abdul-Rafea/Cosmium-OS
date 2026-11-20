#include "terminal.h"

void initializeTerminal(HWND hwnd) {
    g_terminal.window = hwnd;
    SECURITY_ATTRIBUTES saAttr = {};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE; // Allow handles to be inherited by child process

    // 1. Create stdout pipe
    CreatePipe(&g_terminal.hChildStdoutRead, &g_terminal.hChildStdoutWrite, &saAttr, 0);
    SetHandleInformation(g_terminal.hChildStdoutRead, HANDLE_FLAG_INHERIT, 0); // Read handle not inherited

    // 2. Create stdin pipe
    CreatePipe(&g_terminal.hChildStdinRead, &g_terminal.hChildStdinWrite, &saAttr, 0);
    SetHandleInformation(g_terminal.hChildStdinWrite, HANDLE_FLAG_INHERIT, 0); // Write handle not inherited

    // 3. Setup Startup Info to redirect standard handles
    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = g_terminal.hChildStdoutWrite;
    si.hStdError = g_terminal.hChildStdoutWrite;
    si.hStdInput = g_terminal.hChildStdinRead;
    si.wShowWindow = SW_HIDE; // Hides the native black console window

    PROCESS_INFORMATION pi = {};
    wchar_t cmdLine[] = L"cmd.exe /Q";

    // 4. Launch cmd.exe as a background subprocess
    if (CreateProcessW(nullptr, cmdLine, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        g_terminal.running = true;

        // 5. Spin up a background thread to read stdout continuously
        std::thread([]() {
            char buffer[4096];
            DWORD bytesRead;
            bool skippedBanner = false;

            while (g_terminal.running) {
                if (ReadFile(g_terminal.hChildStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';

                    // Convert narrow system bytes to wide string
                    int wideLen = MultiByteToWideChar(CP_ACP, 0, buffer, bytesRead, nullptr, 0);
                    std::wstring wideStr(wideLen, 0);
                    MultiByteToWideChar(CP_ACP, 0, buffer, bytesRead, &wideStr[0], wideLen);

                    // Skiping the initial Windows version and copyright header lines
                    if (!skippedBanner) {
                        // Look for the initial drive prompt (e.g., "C:\") indicating the banner has finished
                        if (wideStr.find(L"\\") != std::wstring::npos || wideStr.find(L">") != std::wstring::npos) {
                            skippedBanner = true;
                        } else {
                            continue; // Skip adding this banner chunk to history
                        }
                    }

                    // Thread-safely push output to history and trigger screen refresh
                    {
                        std::lock_guard<std::mutex> lock(g_terminal.historyMutex);
                        g_terminal.history.push_back(wideStr);
                    }
                    if (g_terminal.window) {
                        PostMessageW(g_terminal.window, WM_APP + 1, 0, 0);
                    }
                } else {
                    break;
                }
            }
        }).detach();
    }
}

void renderTerminal(HWND hwnd) {
    float windowWidth = getWindowWidth(hwnd);
    float windowHeight = getWindowHeight(hwnd);

    // Positioning terminal box below the top icons
    g_terminal.bounds = D2D1::RectF(40.0f, 80.0f, windowWidth - 40.0f, windowHeight - 40.0f);

    // Creating brushes
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> boxBgBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> borderBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> textBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> promptBrush;
    g_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.04f, 0.05f, 0.06f), &boxBgBrush);
    g_d2dContext->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &borderBrush);
    g_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.88f, 0.92f), &textBrush);
    g_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.35f, 0.85f, 0.55f), &promptBrush);

    // Drawing container background and border
    D2D1_ROUNDED_RECT roundedBox = D2D1::RoundedRect(g_terminal.bounds, 8.0f, 8.0f);
    g_d2dContext->FillRoundedRectangle(roundedBox, boxBgBrush.Get());
    g_d2dContext->DrawRoundedRectangle(roundedBox, borderBrush.Get(), 1.0f);

    // Clip rendering to the inside of the box so scrolled text doesn't spill outside
    D2D1_RECT_F innerClip = D2D1::RectF(
        g_terminal.bounds.left + 12.0f,
        g_terminal.bounds.top + 12.0f,
        g_terminal.bounds.right - 12.0f,
        g_terminal.bounds.bottom - 12.0f
    );
    g_d2dContext->PushAxisAlignedClip(innerClip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // Draw history from the top of the terminal downward.
    float currentY = innerClip.top - g_terminal.scrollOffset;
    std::vector<std::wstring> historyCopy;
    {
        std::lock_guard<std::mutex> lock(g_terminal.historyMutex);
        historyCopy = g_terminal.history;
    }

    for (const std::wstring& line : historyCopy) {
        if (currentY > innerClip.bottom) break;

        D2D1_RECT_F lineRect = D2D1::RectF(innerClip.left, currentY, innerClip.right, currentY + g_terminal.lineHeight);
        g_d2dContext->DrawText(
            line.c_str(),
            static_cast<UINT32>(line.length()),
            g_textFormat.Get(),
            lineRect,
            textBrush.Get()
        );
        currentY += g_terminal.lineHeight;
    }

    std::wstring promptLine = L"> " + g_terminal.currentInput + L"_";
    if (currentY <= innerClip.bottom) {
        D2D1_RECT_F lineRect = D2D1::RectF(innerClip.left, currentY, innerClip.right, currentY + g_terminal.lineHeight);
        g_d2dContext->DrawText(promptLine.c_str(), static_cast<UINT32>(promptLine.length()), g_textFormat.Get(), lineRect, promptBrush.Get());
    }

    g_d2dContext->PopAxisAlignedClip();
}

void stopTerminal() {
    g_terminal.running = false;

    if (g_terminal.hChildStdinWrite) {
        CloseHandle(g_terminal.hChildStdinWrite);
        g_terminal.hChildStdinWrite = nullptr;
    }
    if (g_terminal.hChildStdoutRead) {
        CloseHandle(g_terminal.hChildStdoutRead);
        g_terminal.hChildStdoutRead = nullptr;
    }
    if (g_terminal.hChildStdinRead) {
        CloseHandle(g_terminal.hChildStdinRead);
        g_terminal.hChildStdinRead = nullptr;
    }
    if (g_terminal.hChildStdoutWrite) {
        CloseHandle(g_terminal.hChildStdoutWrite);
        g_terminal.hChildStdoutWrite = nullptr;
    }
}