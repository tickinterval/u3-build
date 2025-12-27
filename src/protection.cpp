#include "protection.h"
#include <intrin.h>
#include <tlhelp32.h>
#include <atomic>

namespace protection {

static std::atomic<bool> g_protection_running(true);
static HANDLE g_protection_thread = nullptr;
static HMODULE g_module_handle = nullptr;

static bool EndsWithExe(const wchar_t* value) {
    if (!value) {
        return false;
    }
    size_t len = wcslen(value);
    if (len < 4) {
        return false;
    }
    return wcscmp(value + len - 4, L".exe") == 0;
}

// ================== ANTI-DEBUG ==================

void HideFromDebugger() {
    typedef NTSTATUS(WINAPI* NtSetInformationThread_t)(HANDLE, DWORD, PVOID, ULONG);
    
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return;
    
    auto NtSetInformationThread = reinterpret_cast<NtSetInformationThread_t>(
        GetProcAddress(ntdll, "NtSetInformationThread"));
    
    if (NtSetInformationThread) {
        // ThreadHideFromDebugger = 0x11
        NtSetInformationThread(GetCurrentThread(), 0x11, nullptr, 0);
    }
}

bool IsDebuggerPresent_Check() {
    return ::IsDebuggerPresent() != FALSE;
}

bool CheckRemoteDebugger() {
    BOOL is_debugged = FALSE;
    ::CheckRemoteDebuggerPresent(GetCurrentProcess(), &is_debugged);
    return is_debugged != FALSE;
}

bool CheckPEB() {
#ifdef _WIN64
    BYTE* peb = reinterpret_cast<BYTE*>(__readgsqword(0x60));
#else
    BYTE* peb = reinterpret_cast<BYTE*>(__readfsdword(0x30));
#endif
    return peb && peb[2] != 0;
}

bool CheckHardwareBreakpoints() {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    
    if (GetThreadContext(GetCurrentThread(), &ctx)) {
        return ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0;
    }
    return false;
}

bool CheckNtGlobalFlag() {
#ifdef _WIN64
    BYTE* peb = reinterpret_cast<BYTE*>(__readgsqword(0x60));
    DWORD ntGlobalFlag = *reinterpret_cast<DWORD*>(peb + 0xBC);
#else
    BYTE* peb = reinterpret_cast<BYTE*>(__readfsdword(0x30));
    DWORD ntGlobalFlag = *reinterpret_cast<DWORD*>(peb + 0x68);
#endif
    const DWORD debugFlags = 0x70;
    return (ntGlobalFlag & debugFlags) != 0;
}

bool CheckDebugFlags() {
    typedef NTSTATUS(WINAPI* NtQueryInformationProcess_t)(HANDLE, DWORD, PVOID, ULONG, PULONG);
    
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    
    auto NtQueryInformationProcess = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(ntdll, "NtQueryInformationProcess"));
    
    if (NtQueryInformationProcess) {
        DWORD debugFlags = 0;
        NTSTATUS status = NtQueryInformationProcess(GetCurrentProcess(), 0x1F, &debugFlags, sizeof(DWORD), nullptr);
        if (status >= 0 && debugFlags == 0) {
            return true;
        }
    }
    return false;
}

bool CheckDebugObjectHandle() {
    typedef NTSTATUS(WINAPI* NtQueryInformationProcess_t)(HANDLE, DWORD, PVOID, ULONG, PULONG);
    
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    
    auto NtQueryInformationProcess = reinterpret_cast<NtQueryInformationProcess_t>(
        GetProcAddress(ntdll, "NtQueryInformationProcess"));
    
    if (NtQueryInformationProcess) {
        HANDLE debugObject = nullptr;
        NTSTATUS status = NtQueryInformationProcess(GetCurrentProcess(), 0x1E, &debugObject, sizeof(HANDLE), nullptr);
        if (status >= 0 && debugObject != nullptr) {
            return true;
        }
    }
    return false;
}

bool IsDebuggerDetected() {
    if (IsDebuggerPresent_Check()) return true;
    if (CheckRemoteDebugger()) return true;
    if (CheckPEB()) return true;
    if (CheckHardwareBreakpoints()) return true;
    if (CheckNtGlobalFlag()) return true;
    if (CheckDebugFlags()) return true;
    if (CheckDebugObjectHandle()) return true;
    return false;
}

// Подозрительные процессы для DLL
static const wchar_t* g_suspicious_processes[] = {
    L"x64dbg.exe", L"x32dbg.exe", L"ollydbg.exe",
    L"ida.exe", L"ida64.exe", L"idaq.exe", L"idaq64.exe",
    L"windbg.exe", L"processhacker.exe",
    L"procmon.exe", L"procexp.exe",
    L"cheatengine", L"ce.exe",
    L"scylla", L"importrec",
    L"dnspy.exe", L"de4dot.exe",
};

bool CheckSuspiciousProcesses() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    
    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            wchar_t procName[MAX_PATH];
            wcscpy_s(procName, entry.szExeFile);
            _wcslwr_s(procName);
            
            for (const auto& suspicious : g_suspicious_processes) {
                wchar_t suspLower[MAX_PATH];
                wcscpy_s(suspLower, suspicious);
                _wcslwr_s(suspLower);

                if (EndsWithExe(suspLower)) {
                    if (wcscmp(procName, suspLower) != 0) {
                        continue;
                    }
                } else if (wcsstr(procName, suspLower) == nullptr) {
                    continue;
                }

                found = true;
                break;
            }
            
            if (found) break;
        } while (Process32NextW(snapshot, &entry));
    }
    
    CloseHandle(snapshot);
    return found;
}

// ================== ANTI-DUMP ==================

void ErasePEHeader() {
    if (!g_module_handle) return;
    
    BYTE* base = reinterpret_cast<BYTE*>(g_module_handle);
    
    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    
    DWORD headerSize = nt->OptionalHeader.SizeOfHeaders;
    
    // Меняем защиту памяти
    DWORD oldProtect;
    if (VirtualProtect(base, headerSize, PAGE_READWRITE, &oldProtect)) {
        // Затираем заголовки нулями
        SecureZeroMemory(base, headerSize);
        
        // Восстанавливаем защиту
        VirtualProtect(base, headerSize, oldProtect, &oldProtect);
    }
}

void CorruptPEHeader() {
    if (!g_module_handle) return;
    
    BYTE* base = reinterpret_cast<BYTE*>(g_module_handle);
    
    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;
    
    DWORD headerSize = nt->OptionalHeader.SizeOfHeaders;
    
    DWORD oldProtect;
    if (VirtualProtect(base, headerSize, PAGE_READWRITE, &oldProtect)) {
        // Портим DOS заголовок (кроме e_lfanew)
        dos->e_magic = 0xDEAD;
        
        // Портим PE сигнатуру
        nt->Signature = 0xDEADBEEF;
        
        // Портим количество секций
        nt->FileHeader.NumberOfSections = 0xFF;
        
        // Портим EntryPoint
        nt->OptionalHeader.AddressOfEntryPoint = 0xDEADC0DE;
        
        // Затираем часть опционального заголовка мусором
        BYTE* optionalHeader = reinterpret_cast<BYTE*>(&nt->OptionalHeader);
        for (int i = 0x10; i < 0x40; i++) {
            optionalHeader[i] = static_cast<BYTE>(GetTickCount() & 0xFF);
        }
        
        VirtualProtect(base, headerSize, oldProtect, &oldProtect);
    }
}

// Структуры PEB для манипуляции
typedef struct _UNICODE_STRING_PEB {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING_PEB;

typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING_PEB FullDllName;
    UNICODE_STRING_PEB BaseDllName;
    // ... остальные поля не нужны
} LDR_DATA_TABLE_ENTRY;

typedef struct _PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    HANDLE SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA;

void UnlinkFromPEB() {
    if (!g_module_handle) return;
    
#ifdef _WIN64
    BYTE* peb = reinterpret_cast<BYTE*>(__readgsqword(0x60));
    PEB_LDR_DATA* ldr = *reinterpret_cast<PEB_LDR_DATA**>(peb + 0x18);
#else
    BYTE* peb = reinterpret_cast<BYTE*>(__readfsdword(0x30));
    PEB_LDR_DATA* ldr = *reinterpret_cast<PEB_LDR_DATA**>(peb + 0x0C);
#endif
    
    if (!ldr) return;
    
    // Проходим по всем трём спискам
    LIST_ENTRY* lists[] = {
        &ldr->InLoadOrderModuleList,
        &ldr->InMemoryOrderModuleList,
        &ldr->InInitializationOrderModuleList
    };
    
    for (int listIndex = 0; listIndex < 3; listIndex++) {
        LIST_ENTRY* head = lists[listIndex];
        LIST_ENTRY* current = head->Flink;
        
        while (current != head) {
            LDR_DATA_TABLE_ENTRY* entry = nullptr;
            
            switch (listIndex) {
                case 0: // InLoadOrderLinks
                    entry = CONTAINING_RECORD(current, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
                    break;
                case 1: // InMemoryOrderLinks
                    entry = CONTAINING_RECORD(current, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
                    break;
                case 2: // InInitializationOrderLinks
                    entry = CONTAINING_RECORD(current, LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks);
                    break;
            }
            
            if (entry && entry->DllBase == g_module_handle) {
                // Отвязываем модуль из списка
                current->Blink->Flink = current->Flink;
                current->Flink->Blink = current->Blink;
                break;
            }
            
            current = current->Flink;
        }
    }
}

// ================== INTEGRITY ==================

bool CheckApiHooks() {
    auto CheckForHook = [](void* func) -> bool {
        if (!func) return false;
        
        BYTE* bytes = static_cast<BYTE*>(func);
        
        if (bytes[0] == 0xE9 || bytes[0] == 0xE8) return true;
        if (bytes[0] == 0x68 && bytes[5] == 0xC3) return true;
        if (bytes[0] == 0xFF && bytes[1] == 0x25) return true;
#ifdef _WIN64
        if (bytes[0] == 0x48 && bytes[1] == 0xB8) return true;
#endif
        
        return false;
    };
    
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    
    if (!ntdll || !kernel32) return false;
    
    // Примечание: Мы сами хукаем некоторые функции через MinHook,
    // поэтому проверяем только критические системные функции
    if (CheckForHook(GetProcAddress(ntdll, "NtQueryInformationProcess"))) return true;
    if (CheckForHook(GetProcAddress(kernel32, "IsDebuggerPresent"))) return true;
    
    return false;
}

// ================== WATCHDOG ==================

static void CrashProcess() {
    // Различные методы краша для усложнения анализа
    volatile int* null = nullptr;
    *null = 0;
}

DWORD WINAPI ProtectionThread(LPVOID param) {
    // Скрываем этот поток от отладчика
    HideFromDebugger();
    
    // Начальная задержка
    Sleep(2000 + (GetTickCount() % 1000));
    
    // Anti-dump: затираем заголовки после инициализации
    CorruptPEHeader();
    
    // Отвязываем модуль из PEB
    UnlinkFromPEB();
    
    DWORD counter = 0;
    
    while (g_protection_running) {
        bool detected = false;
        
        // Разные проверки каждую итерацию
        switch (counter % 5) {
            case 0:
                if (IsDebuggerPresent_Check()) {
                    detected = true;
                    break;
                }
                if (CheckRemoteDebugger()) {
                    detected = true;
                }
                break;
            case 1:
                if (CheckPEB()) {
                    detected = true;
                    break;
                }
                if (CheckHardwareBreakpoints()) {
                    detected = true;
                }
                break;
            case 2:
                if (CheckNtGlobalFlag()) {
                    detected = true;
                    break;
                }
                if (CheckDebugFlags()) {
                    detected = true;
                }
                break;
            case 3:
                if (CheckDebugObjectHandle()) {
                    detected = true;
                }
                break;
            case 4:
                if (CheckSuspiciousProcesses()) {
                    detected = true;
                }
                break;
        }
        
        if (detected) {
            CrashProcess();
        }
        
        counter++;
        
        // Рандомный интервал 2-4 секунды (реже чем в лоадере, чтобы не влиять на производительность)
        Sleep(2000 + (GetTickCount() % 2000));
    }
    
    return 0;
}

void StartProtectionThread() {
    g_protection_running = true;
    g_protection_thread = CreateThread(nullptr, 0, ProtectionThread, nullptr, 0, nullptr);
    
    if (g_protection_thread) {
        SetThreadPriority(g_protection_thread, THREAD_PRIORITY_BELOW_NORMAL);
    }
}

void StopProtectionThread() {
    g_protection_running = false;
    
    if (g_protection_thread) {
        WaitForSingleObject(g_protection_thread, 3000);
        CloseHandle(g_protection_thread);
        g_protection_thread = nullptr;
    }
}

// ================== INITIALIZATION ==================

void Initialize() {
    // Получаем handle нашего модуля
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(&Initialize, &mbi, sizeof(mbi))) {
        g_module_handle = reinterpret_cast<HMODULE>(mbi.AllocationBase);
    }
    
    // Скрываем основной поток
    HideFromDebugger();
    
    // Начальная проверка
    if (IsDebuggerDetected()) {
        CrashProcess();
    }
    
    // Запускаем защитный поток
    StartProtectionThread();
}

void Shutdown() {
    StopProtectionThread();
}

} // namespace protection




