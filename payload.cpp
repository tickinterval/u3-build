#include <windows.h>

extern "C" __declspec(dllexport) void Run() {
    MessageBoxW(nullptr, L"Payload executed", L"Loader", MB_OK | MB_ICONINFORMATION);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)instance;
    (void)reason;
    (void)reserved;
    return TRUE;
}
