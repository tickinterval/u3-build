#pragma once

#include <windows.h>
#include <winhttp.h>
#include <psapi.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "psapi.lib")

// Динамическая загрузка функций для скрытия импортов
namespace dynamic_imports {

// Инициализация - загрузка всех необходимых функций
bool Initialize();

// Обёртки для критических функций
namespace kernel32 {
    typedef HMODULE(WINAPI* LoadLibraryA_t)(LPCSTR);
    typedef FARPROC(WINAPI* GetProcAddress_t)(HMODULE, LPCSTR);
    typedef LPVOID(WINAPI* VirtualAlloc_t)(LPVOID, SIZE_T, DWORD, DWORD);
    typedef BOOL(WINAPI* VirtualFree_t)(LPVOID, SIZE_T, DWORD);
    typedef BOOL(WINAPI* VirtualProtect_t)(LPVOID, SIZE_T, DWORD, PDWORD);
    typedef HANDLE(WINAPI* CreateThread_t)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
    typedef VOID(WINAPI* Sleep_t)(DWORD);
    typedef ULONGLONG(WINAPI* GetTickCount64_t)(VOID);
    typedef DWORD(WINAPI* GetCurrentProcessId_t)(VOID);
    typedef HMODULE(WINAPI* GetModuleHandleW_t)(LPCWSTR);
    typedef BOOL(WINAPI* GetModuleInformation_t)(HANDLE, HMODULE, LPMODULEINFO, DWORD);
    
    extern LoadLibraryA_t LoadLibraryA;
    extern GetProcAddress_t GetProcAddress;
    extern VirtualAlloc_t VirtualAlloc;
    extern VirtualFree_t VirtualFree;
    extern VirtualProtect_t VirtualProtect;
    extern CreateThread_t CreateThread;
    extern Sleep_t Sleep;
    extern GetTickCount64_t GetTickCount64;
    extern GetCurrentProcessId_t GetCurrentProcessId;
    extern GetModuleHandleW_t GetModuleHandleW;
    extern GetModuleInformation_t GetModuleInformation;
}

namespace ntdll {
    typedef LONG NTSTATUS;
    typedef NTSTATUS(WINAPI* NtSetInformationThread_t)(HANDLE, DWORD, PVOID, ULONG);
    typedef NTSTATUS(WINAPI* NtQueryInformationProcess_t)(HANDLE, DWORD, PVOID, ULONG, PULONG);
    
    extern NtSetInformationThread_t NtSetInformationThread;
    extern NtQueryInformationProcess_t NtQueryInformationProcess;
}

namespace winhttp {
    typedef HINTERNET(WINAPI* WinHttpOpen_t)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
    typedef HINTERNET(WINAPI* WinHttpConnect_t)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);
    typedef HINTERNET(WINAPI* WinHttpOpenRequest_t)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
    typedef BOOL(WINAPI* WinHttpSendRequest_t)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);
    typedef BOOL(WINAPI* WinHttpReceiveResponse_t)(HINTERNET, LPVOID);
    typedef BOOL(WINAPI* WinHttpReadData_t)(HINTERNET, LPVOID, DWORD, LPDWORD);
    typedef BOOL(WINAPI* WinHttpQueryDataAvailable_t)(HINTERNET, LPDWORD);
    typedef BOOL(WINAPI* WinHttpCloseHandle_t)(HINTERNET);
    
    extern WinHttpOpen_t WinHttpOpen;
    extern WinHttpConnect_t WinHttpConnect;
    extern WinHttpOpenRequest_t WinHttpOpenRequest;
    extern WinHttpSendRequest_t WinHttpSendRequest;
    extern WinHttpReceiveResponse_t WinHttpReceiveResponse;
    extern WinHttpReadData_t WinHttpReadData;
    extern WinHttpQueryDataAvailable_t WinHttpQueryDataAvailable;
    extern WinHttpCloseHandle_t WinHttpCloseHandle;
}

} // namespace dynamic_imports

