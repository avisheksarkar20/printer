#include <windows.h>

#include "MainWindow.h"

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE /*hPrevInstance*/,
                      LPWSTR    /*lpCmdLine*/,
                      int       nCmdShow) {
    MainWindow window;
    if (!window.Create(hInstance, nCmdShow)) {
        ::MessageBoxW(nullptr,
                      L"Failed to create main window.",
                      L"Print Queue Manager",
                      MB_OK | MB_ICONERROR);
        return 1;
    }

    HACCEL hAccel = nullptr;
    MSG msg{};
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (hAccel && ::TranslateAcceleratorW(window.Handle(), hAccel, &msg)) {
            continue;
        }
        if (!::IsDialogMessageW(window.Handle(), &msg)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }
    return static_cast<int>(msg.wParam);
}
