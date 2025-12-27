#include "integrity_check.h"
#include <intrin.h>
#include <psapi.h>
#include <cstring>
#include <string>
#include <cstdlib>

#pragma comment(lib, "psapi.lib")

namespace integrity_check {

// Магические константы для overlay
constexpr const char* PROTECTION_MAGIC = "U3PR1";
constexpr uint8_t PROTECTION_VERSION = 1;

// Получение CPUID
static uint32_t GetCurrentCPUID() {
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    return static_cast<uint32_t>(cpuInfo[0] ^ cpuInfo[1] ^ cpuInfo[2] ^ cpuInfo[3]);
}

// Чтение overlay из конца DLL
static bool ReadOverlay(HMODULE module, BYTE** data, size_t* size) {
    if (!module) {
        return false;
    }
    
    // Получаем информацию о модуле
    MODULEINFO modInfo = {};
    if (!GetModuleInformation(GetCurrentProcess(), module, &modInfo, sizeof(modInfo))) {
        return false;
    }
    
    // Ищем overlay в конце образа
    // Overlay обычно находится после всех секций
    BYTE* base = reinterpret_cast<BYTE*>(modInfo.lpBaseOfDll);
    size_t imageSize = modInfo.SizeOfImage;
    
    // Ищем с конца образа
    // Overlay может быть в последних 8KB
    const size_t searchSize = 8192;
    size_t startPos = (imageSize > searchSize) ? (imageSize - searchSize) : 0;
    
    for (size_t i = startPos; i < imageSize - 10; i++) {
        BYTE* pos = base + i;
        
        // Проверяем магическое число
        if (memcmp(pos, PROTECTION_MAGIC, 5) == 0) {
            // Проверяем версию
            if (pos[5] == PROTECTION_VERSION) {
                // Читаем длину
                uint32_t dataLength = *reinterpret_cast<uint32_t*>(pos + 6);
                
                if (dataLength > 0 && dataLength < 4096 && (i + 10 + dataLength) <= imageSize) {
                    *data = pos + 10;
                    *size = dataLength;
                    return true;
                }
            }
        }
    }
    
    return false;
}

// Простой парсинг JSON (только для нашей структуры)
static bool ParseProtectionJson(const BYTE* data, size_t size, ProtectionData* out) {
    if (!data || !out || size == 0) {
        return false;
    }
    
    // Простой поиск значений в JSON
    std::string json(reinterpret_cast<const char*>(data), size);
    
    // Ищем cpuid
    size_t cpuidPos = json.find("\"cpuid\":");
    if (cpuidPos != std::string::npos) {
        cpuidPos += 8;
        while (cpuidPos < json.size() && (json[cpuidPos] == ' ' || json[cpuidPos] == '\t')) {
            cpuidPos++;
        }
        if (cpuidPos < json.size()) {
            out->cpuid = static_cast<uint32_t>(strtoul(&json[cpuidPos], nullptr, 10));
        }
    }
    
    // Ищем timestamp
    size_t timestampPos = json.find("\"timestamp\":");
    if (timestampPos != std::string::npos) {
        timestampPos += 12;
        while (timestampPos < json.size() && (json[timestampPos] == ' ' || json[timestampPos] == '\t')) {
            timestampPos++;
        }
        if (timestampPos < json.size()) {
            out->timestamp = static_cast<uint64_t>(strtoull(&json[timestampPos], nullptr, 10));
        }
    }
    
    // Ищем process_id
    size_t processIdPos = json.find("\"process_id\":");
    if (processIdPos != std::string::npos) {
        processIdPos += 13;
        while (processIdPos < json.size() && (json[processIdPos] == ' ' || json[processIdPos] == '\t')) {
            processIdPos++;
        }
        if (processIdPos < json.size()) {
            out->process_id = static_cast<uint32_t>(strtoul(&json[processIdPos], nullptr, 10));
        }
    }
    
    return (out->cpuid != 0 || out->timestamp != 0);
}

bool ReadProtectionData(HMODULE module, ProtectionData* data) {
    if (!data) {
        return false;
    }
    
    memset(data, 0, sizeof(ProtectionData));
    
    BYTE* overlayData = nullptr;
    size_t overlaySize = 0;
    
    if (!ReadOverlay(module, &overlayData, &overlaySize)) {
        return false;
    }
    
    return ParseProtectionJson(overlayData, overlaySize, data);
}

bool VerifyCPUID(uint32_t expected_cpuid) {
    if (expected_cpuid == 0) {
        return true; // Не проверяем если не задано
    }
    
    uint32_t current = GetCurrentCPUID();
    return (current == expected_cpuid);
}

bool VerifyTimestamp(uint64_t expected_timestamp) {
    if (expected_timestamp == 0) {
        return true; // Не проверяем если не задано
    }
    
    // Timestamp должен быть не старше 5 минут (300000 мс)
    const uint64_t MAX_AGE_MS = 5 * 60 * 1000;
    uint64_t currentTime = GetTickCount64();
    
    if (expected_timestamp > currentTime) {
        return false; // Timestamp из будущего
    }
    
    uint64_t age = currentTime - expected_timestamp;
    return (age <= MAX_AGE_MS);
}

bool VerifyProcessAddresses(HMODULE module) {
    // Базовая проверка - проверяем что мы в правильном процессе
    // Более детальная проверка адресов модулей будет добавлена позже
    // Пока просто проверяем что process_id совпадает
    ProtectionData data = {};
    if (!ReadProtectionData(module, &data)) {
        return false;
    }
    
    if (data.process_id != 0) {
        DWORD currentPid = GetCurrentProcessId();
        return (currentPid == data.process_id);
    }
    
    return true;
}

bool VerifyAll(HMODULE module) {
    ProtectionData data = {};
    if (!ReadProtectionData(module, &data)) {
        // Если нет данных защиты, разрешаем (для обратной совместимости)
        return true;
    }
    
    // Проверяем CPUID
    if (!VerifyCPUID(data.cpuid)) {
        return false;
    }
    
    // Проверяем timestamp
    if (!VerifyTimestamp(data.timestamp)) {
        return false;
    }
    
    // Проверяем адреса процесса
    if (!VerifyProcessAddresses(module)) {
        return false;
    }
    
    return true;
}

} // namespace integrity_check

