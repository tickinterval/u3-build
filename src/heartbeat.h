#pragma once

#include <windows.h>
#include <string>

namespace heartbeat {

// Конфигурация heartbeat
struct HeartbeatConfig {
    std::wstring server_url;      // URL сервера (например: https://u3.llvm.uk)
    std::wstring license_key;     // Лицензионный ключ
    std::wstring hwid;            // Hardware ID
    std::wstring product_code;    // Код продукта
    std::string event_token;      // Токен для событий
    DWORD interval_ms;            // Интервал проверки в мс (например: 60000 = 1 мин)
};

// Инициализация heartbeat системы
// Данные передаются из лоадера через watermark или другой механизм
void Initialize(const HeartbeatConfig& config);

// Альтернативная инициализация: читает watermark из PE
// Watermark содержит зашифрованные данные конфигурации
void InitializeFromWatermark();

// Остановка heartbeat
void Shutdown();

// Проверка статуса
bool IsRunning();

// Callback при отзыве ключа (по умолчанию - краш процесса)
typedef void (*OnRevokedCallback)();
void SetOnRevokedCallback(OnRevokedCallback callback);

// Callback при потере связи (опционально)
typedef void (*OnConnectionLostCallback)(int failCount);
void SetOnConnectionLostCallback(OnConnectionLostCallback callback);

} // namespace heartbeat


