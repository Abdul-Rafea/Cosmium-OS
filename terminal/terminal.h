#pragma once

#include <windows.h>
#include <d2d1.h>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <atomic>

#include "../direct-2D.h"

// Structs
struct Terminal {
    HANDLE hChildStdinRead = nullptr;
    HANDLE hChildStdinWrite = nullptr;
    HANDLE hChildStdoutRead = nullptr;
    HANDLE hChildStdoutWrite = nullptr;
    
    std::vector<std::wstring> history = {};
    std::wstring currentInput = L"";
    float scrollOffset = 0.0f;
    float lineHeight = 24.0f;
    D2D1_RECT_F bounds = {};

    std::mutex historyMutex;
    std::atomic<bool> running{ false };
    HWND window = nullptr;
};

// Global Direct 2D Variables
inline Terminal g_terminal;

// Functions
void initializeTerminal(HWND hwnd);
void renderTerminal(HWND hwnd, float windowWidth, float windowHeight);
void stopTerminal();