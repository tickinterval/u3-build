#pragma once

#include <windows.h>
#include <cstdint>

namespace integrity_check {

// Структура защиты из overlay
struct ProtectionData {
    uint32_t cpuid;
    uint64_t timestamp;
    uint32_t process_id;
    // Модули и функции будут проверяться отдельно
};

// Чтение данных защиты из overlay DLL
bool ReadProtectionData(HMODULE module, ProtectionData* data);

// Проверка CPUID
bool VerifyCPUID(uint32_t expected_cpuid);

// Проверка timestamp (не должен быть старше 5 минут)
bool VerifyTimestamp(uint64_t expected_timestamp);

// Проверка адресов процесса
bool VerifyProcessAddresses(HMODULE module);

// Комплексная проверка всех значений
bool VerifyAll(HMODULE module);

} // namespace integrity_check


