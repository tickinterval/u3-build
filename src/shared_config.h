#pragma once

#include <windows.h>
#include <string>

namespace shared_config {

// Имя shared memory (с рандомным суффиксом для уникальности)
#define SHARED_CONFIG_NAME_PREFIX L"Local\\U3W_CFG_"

// Магическое число для проверки валидности
#define SHARED_CONFIG_MAGIC 0x55335743  // "U3WC"

// Максимальные размеры строк
#define MAX_URL_LENGTH 256
#define MAX_KEY_LENGTH 64
#define MAX_HWID_LENGTH 128
#define MAX_PRODUCT_LENGTH 32
#define MAX_TOKEN_LENGTH 512

// Структура конфигурации в shared memory
#pragma pack(push, 1)
struct SharedConfig {
    DWORD magic;                            // Магическое число для проверки
    DWORD version;                          // Версия структуры
    wchar_t server_url[MAX_URL_LENGTH];     // URL сервера
    wchar_t license_key[MAX_KEY_LENGTH];    // Лицензионный ключ
    wchar_t hwid[MAX_HWID_LENGTH];          // Hardware ID
    wchar_t product_code[MAX_PRODUCT_LENGTH]; // Код продукта
    char event_token[MAX_TOKEN_LENGTH];     // Токен для событий
    DWORD heartbeat_interval_ms;            // Интервал heartbeat в мс
    DWORD flags;                            // Дополнительные флаги
};
#pragma pack(pop)

// Флаги
#define CONFIG_FLAG_HEARTBEAT_ENABLED   0x0001
#define CONFIG_FLAG_PROTECTION_ENABLED  0x0002

// Генерация имени для текущего процесса
inline std::wstring GenerateSharedName() {
    wchar_t name[64];
    swprintf_s(name, L"%s%08X", SHARED_CONFIG_NAME_PREFIX, GetCurrentProcessId());
    return name;
}

// Чтение конфигурации из shared memory (вызывается DLL)
inline bool ReadConfig(SharedConfig* config) {
    std::wstring name = GenerateSharedName();
    
    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str());
    if (!mapping) {
        return false;
    }
    
    SharedConfig* shared = reinterpret_cast<SharedConfig*>(
        MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(SharedConfig))
    );
    
    if (!shared) {
        CloseHandle(mapping);
        return false;
    }
    
    // Проверяем магическое число
    if (shared->magic != SHARED_CONFIG_MAGIC) {
        UnmapViewOfFile(shared);
        CloseHandle(mapping);
        return false;
    }
    
    // Копируем данные
    memcpy(config, shared, sizeof(SharedConfig));
    
    UnmapViewOfFile(shared);
    CloseHandle(mapping);
    
    return true;
}

} // namespace shared_config

