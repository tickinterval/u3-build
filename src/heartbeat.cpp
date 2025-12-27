#include "heartbeat.h"
#include "integrity_check.h"
#include <winhttp.h>
#include <atomic>
#include <thread>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace heartbeat {

static std::atomic<bool> g_running(false);
static HANDLE g_thread = nullptr;
static HeartbeatConfig g_config;
static OnRevokedCallback g_on_revoked = nullptr;
static OnConnectionLostCallback g_on_connection_lost = nullptr;

// Максимум неудачных попыток перед вызовом callback
static const int MAX_FAIL_COUNT = 5;

// Дефолтный callback при отзыве - краш
static void DefaultOnRevoked() {
    // Различные методы завершения для усложнения анализа
    volatile int* null = nullptr;
    *null = 0xDEAD;
}

// Преобразование wstring в string (UTF-8)
static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

// Простой JSON escape
static std::string JsonEscape(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 16);
    for (char c : value) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

// Парсинг URL для WinHTTP
static bool ParseUrl(const std::wstring& url, std::wstring& host, std::wstring& path, bool& secure, WORD& port) {
    URL_COMPONENTS components = {};
    components.dwStructSize = sizeof(components);
    
    wchar_t hostBuf[256] = {};
    wchar_t pathBuf[1024] = {};
    
    components.lpszHostName = hostBuf;
    components.dwHostNameLength = 256;
    components.lpszUrlPath = pathBuf;
    components.dwUrlPathLength = 1024;
    
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &components)) {
        return false;
    }
    
    host = hostBuf;
    path = pathBuf;
    secure = (components.nScheme == INTERNET_SCHEME_HTTPS);
    port = components.nPort;
    
    return true;
}

// HTTP POST запрос
static bool HttpPost(const std::wstring& url, const std::string& body, std::string& response) {
    std::wstring host, path;
    bool secure;
    WORD port;
    
    if (!ParseUrl(url, host, path, secure, port)) {
        return false;
    }
    
    HINTERNET session = WinHttpOpen(L"u3ware/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return false;
    }
    
    // Таймауты
    DWORD timeout = 10000; // 10 секунд
    WinHttpSetTimeouts(session, timeout, timeout, timeout, timeout);
    
    HINTERNET connection = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        return false;
    }
    
    DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection, L"POST", path.c_str(), 
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }
    
    // TLS: rely on default certificate validation (do not ignore cert errors).
    // Content-Type
    const wchar_t* headers = L"Content-Type: application/json\r\n";
    
    BOOL result = WinHttpSendRequest(request, headers, -1, 
        const_cast<char*>(body.c_str()), static_cast<DWORD>(body.size()), 
        static_cast<DWORD>(body.size()), 0);
    
    if (!result) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }
    
    result = WinHttpReceiveResponse(request, nullptr);
    if (!result) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }
    
    // Проверяем статус код
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
    
    // Читаем ответ
    response.clear();
    DWORD bytesAvailable = 0;
    
    while (WinHttpQueryDataAvailable(request, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable + 1, 0);
        DWORD bytesRead = 0;
        if (WinHttpReadData(request, buffer.data(), bytesAvailable, &bytesRead)) {
            response.append(buffer.data(), bytesRead);
        }
    }
    
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    
    // 403 = ключ отозван
    if (statusCode == 403) {
        return false;
    }
    
    return (statusCode >= 200 && statusCode < 300);
}

// Простой JSON парсер для "ok": true/false
static bool ParseOkFromJson(const std::string& json, bool& ok) {
    size_t pos = json.find("\"ok\"");
    if (pos == std::string::npos) return false;
    
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    
    // Пропускаем пробелы
    while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    if (pos >= json.size()) return false;
    
    if (json.substr(pos, 4) == "true") {
        ok = true;
        return true;
    }
    if (json.substr(pos, 5) == "false") {
        ok = false;
        return true;
    }
    
    return false;
}

// Проверка "error": "revoked" или "invalid_key"
static bool IsRevoked(const std::string& json) {
    return (json.find("\"revoked\"") != std::string::npos ||
            json.find("\"invalid_key\"") != std::string::npos ||
            json.find("\"expired\"") != std::string::npos);
}

// Поток heartbeat
static DWORD WINAPI HeartbeatThread(LPVOID param) {
    int failCount = 0;
    
    // Получаем handle нашего модуля для проверки целостности
    HMODULE currentModule = nullptr;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(&HeartbeatThread, &mbi, sizeof(mbi))) {
        currentModule = reinterpret_cast<HMODULE>(mbi.AllocationBase);
    }
    
    // Начальная задержка (рандомная, 5-15 секунд)
    Sleep(5000 + (GetTickCount() % 10000));
    
    int integrityCheckCounter = 0;
    
    while (g_running) {
        // Периодическая проверка целостности DLL (каждые 5 итераций)
        integrityCheckCounter++;
        if (integrityCheckCounter >= 5 && currentModule) {
            integrityCheckCounter = 0;
            if (!integrity_check::VerifyAll(currentModule)) {
                // Целостность нарушена - завершаем работу
                if (g_on_revoked) {
                    g_on_revoked();
                } else {
                    DefaultOnRevoked();
                }
                return 0;
            }
        }
        // Формируем запрос
        std::string body = "{\"key\":\"" + JsonEscape(WideToUtf8(g_config.license_key)) + 
                           "\",\"hwid\":\"" + JsonEscape(WideToUtf8(g_config.hwid)) + 
                           "\",\"type\":\"heartbeat\"";
        
        if (!g_config.product_code.empty()) {
            body += ",\"product_code\":\"" + JsonEscape(WideToUtf8(g_config.product_code)) + "\"";
        }
        
        if (!g_config.event_token.empty()) {
            body += ",\"token\":\"" + JsonEscape(g_config.event_token) + "\"";
        }
        
        body += "}";
        
        std::wstring url = g_config.server_url;
        if (!url.empty() && url.back() != L'/') {
            url += L'/';
        }
        url += L"event";
        
        std::string response;
        bool success = HttpPost(url, body, response);
        
        if (!success) {
            failCount++;
            
            // Проверяем на отзыв
            if (IsRevoked(response)) {
                // Ключ отозван - вызываем callback
                if (g_on_revoked) {
                    g_on_revoked();
                } else {
                    DefaultOnRevoked();
                }
                return 0;
            }
            
            // Уведомляем о потере связи
            if (g_on_connection_lost) {
                g_on_connection_lost(failCount);
            }
            
            // После MAX_FAIL_COUNT неудач подряд - считаем что что-то не так
            if (failCount >= MAX_FAIL_COUNT) {
                if (g_on_revoked) {
                    g_on_revoked();
                } else {
                    DefaultOnRevoked();
                }
                return 0;
            }
        } else {
            failCount = 0;
            
            // Проверяем ответ
            bool ok = false;
            if (ParseOkFromJson(response, ok)) {
                if (!ok && IsRevoked(response)) {
                    if (g_on_revoked) {
                        g_on_revoked();
                    } else {
                        DefaultOnRevoked();
                    }
                    return 0;
                }
            }
        }
        
        // Ждём интервал с небольшой рандомизацией
        DWORD interval = g_config.interval_ms;
        if (interval < 10000) interval = 60000; // Минимум 1 минута
        
        // Добавляем рандом ±10%
        DWORD jitter = interval / 10;
        DWORD actualInterval = interval - jitter + (GetTickCount() % (jitter * 2));
        
        // Спим небольшими интервалами чтобы можно было остановить
        DWORD slept = 0;
        while (slept < actualInterval && g_running) {
            Sleep(1000);
            slept += 1000;
        }
    }
    
    return 0;
}

void Initialize(const HeartbeatConfig& config) {
    if (g_running) {
        return; // Уже запущен
    }
    
    g_config = config;
    g_running = true;
    
    g_thread = CreateThread(nullptr, 0, HeartbeatThread, nullptr, 0, nullptr);
    if (g_thread) {
        // Низкий приоритет чтобы не влиять на производительность
        SetThreadPriority(g_thread, THREAD_PRIORITY_LOWEST);
    }
}

void InitializeFromWatermark() {
    // Получаем базовый адрес DLL
    HMODULE module = nullptr;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(&InitializeFromWatermark, &mbi, sizeof(mbi))) {
        module = reinterpret_cast<HMODULE>(mbi.AllocationBase);
    }
    
    if (!module) {
        return;
    }
    
    // Ищем watermark в конце файла
    // Формат: "U3WM1" (5 байт) + version (1 байт) + length (4 байта LE) + JSON data
    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return;
    }
    
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(
        reinterpret_cast<BYTE*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return;
    }
    
    // Находим последнюю секцию
    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
    DWORD lastSectionEnd = 0;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        DWORD sectionEnd = section[i].VirtualAddress + section[i].Misc.VirtualSize;
        if (sectionEnd > lastSectionEnd) {
            lastSectionEnd = sectionEnd;
        }
    }
    
    // Watermark должен быть после всех секций (в overlay)
    // Для manual mapped DLL это не работает, нужен другой подход
    // Оставляем пустую реализацию - данные должны передаваться через Initialize()
}

void Shutdown() {
    g_running = false;
    
    if (g_thread) {
        WaitForSingleObject(g_thread, 5000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
}

bool IsRunning() {
    return g_running;
}

void SetOnRevokedCallback(OnRevokedCallback callback) {
    g_on_revoked = callback;
}

void SetOnConnectionLostCallback(OnConnectionLostCallback callback) {
    g_on_connection_lost = callback;
}

} // namespace heartbeat



