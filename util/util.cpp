#include "util.h"

float getWindowWidth(HWND hwnd) {
    RECT window;
    GetClientRect(hwnd, &window);
    return static_cast<float>(window.right - window.left);
}

float getWindowHeight(HWND hwnd) {
    RECT window;
    GetClientRect(hwnd, &window);
    return static_cast<float>(window.bottom - window.top);
}