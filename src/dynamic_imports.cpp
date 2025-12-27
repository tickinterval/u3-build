#include "dynamic_imports.h"
#include <psapi.h>

namespace dynamic_imports {

// Реализация для kernel32
namespace kernel32 {
    LoadLibraryA_t LoadLibraryA = nullptr;
    GetProcAddress_t GetProcAddress = nullptr;
    VirtualAlloc_t VirtualAlloc = nullptr;
    VirtualFree_t VirtualFree = nullptr;
    VirtualProtect_t VirtualProtect = nullptr;
    CreateThread_t CreateThread = nullptr;
    Sleep_t Sleep = nullptr;
    GetTickCount64_t GetTickCount64 = nullptr;
    GetCurrentProcessId_t GetCurrentProcessId = nullptr;
    GetModuleHandleW_t GetModuleHandleW = nullptr;
    GetModuleInformation_t GetModuleInformation = nullptr;
}

// Реализация для ntdll
namespace ntdll {
    NtSetInformationThread_t NtSetInformationThread = nullptr;
    NtQueryInformationProcess_t NtQueryInformationProcess = nullptr;
}

// Реализация для winhttp
namespace winhttp {
    WinHttpOpen_t WinHttpOpen = nullptr;
    WinHttpConnect_t WinHttpConnect = nullptr;
    WinHttpOpenRequest_t WinHttpOpenRequest = nullptr;
    WinHttpSendRequest_t WinHttpSendRequest = nullptr;
    WinHttpReceiveResponse_t WinHttpReceiveResponse = nullptr;
    WinHttpReadData_t WinHttpReadData = nullptr;
    WinHttpQueryDataAvailable_t WinHttpQueryDataAvailable = nullptr;
    WinHttpCloseHandle_t WinHttpCloseHandle = nullptr;
}

// Загрузка функции из модуля
template<typename T>
static bool LoadFunction(HMODULE module, const char* name, T*& func) {
    if (!module) {
        return false;
    }
    
    // Используем стандартный GetProcAddress для загрузки
    // (это единственная функция, которую мы можем использовать напрямую)
    FARPROC proc = ::GetProcAddress(module, name);
    if (!proc) {
        return false;
    }
    
    func = reinterpret_cast<T*>(proc);
    return true;
}

bool Initialize() {
    // Загружаем kernel32.dll
    HMODULE kernel32 = ::GetModuleHandleW(L"kernel32.dll");
    if (!kernel32) {
        return false;
    }
    
    // Загружаем функции kernel32
    if (!LoadFunction(kernel32, "LoadLibraryA", kernel32::LoadLibraryA)) return false;
    if (!LoadFunction(kernel32, "GetProcAddress", kernel32::GetProcAddress)) return false;
    if (!LoadFunction(kernel32, "VirtualAlloc", kernel32::VirtualAlloc)) return false;
    if (!LoadFunction(kernel32, "VirtualFree", kernel32::VirtualFree)) return false;
    if (!LoadFunction(kernel32, "VirtualProtect", kernel32::VirtualProtect)) return false;
    if (!LoadFunction(kernel32, "CreateThread", kernel32::CreateThread)) return false;
    if (!LoadFunction(kernel32, "Sleep", kernel32::Sleep)) return false;
    if (!LoadFunction(kernel32, "GetTickCount64", kernel32::GetTickCount64)) return false;
    if (!LoadFunction(kernel32, "GetCurrentProcessId", kernel32::GetCurrentProcessId)) return false;
    if (!LoadFunction(kernel32, "GetModuleHandleW", kernel32::GetModuleHandleW)) return false;
    
    // Загружаем GetModuleInformation из psapi.dll
    HMODULE psapi = kernel32::LoadLibraryA ? kernel32::LoadLibraryA("psapi.dll") : nullptr;
    if (psapi) {
        LoadFunction(psapi, "GetModuleInformation", kernel32::GetModuleInformation);
    }
    
    // Загружаем ntdll.dll
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        LoadFunction(ntdll, "NtSetInformationThread", ntdll::NtSetInformationThread);
        LoadFunction(ntdll, "NtQueryInformationProcess", ntdll::NtQueryInformationProcess);
    }
    
    // Загружаем winhttp.dll
    HMODULE winhttp = kernel32::LoadLibraryA ? kernel32::LoadLibraryA("winhttp.dll") : nullptr;
    if (winhttp) {
        LoadFunction(winhttp, "WinHttpOpen", winhttp::WinHttpOpen);
        LoadFunction(winhttp, "WinHttpConnect", winhttp::WinHttpConnect);
        LoadFunction(winhttp, "WinHttpOpenRequest", winhttp::WinHttpOpenRequest);
        LoadFunction(winhttp, "WinHttpSendRequest", winhttp::WinHttpSendRequest);
        LoadFunction(winhttp, "WinHttpReceiveResponse", winhttp::WinHttpReceiveResponse);
        LoadFunction(winhttp, "WinHttpReadData", winhttp::WinHttpReadData);
        LoadFunction(winhttp, "WinHttpQueryDataAvailable", winhttp::WinHttpQueryDataAvailable);
        LoadFunction(winhttp, "WinHttpCloseHandle", winhttp::WinHttpCloseHandle);
    }
    
    return true;
}

} // namespace dynamic_imports

