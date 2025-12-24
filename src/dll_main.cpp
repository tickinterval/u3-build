#include <windows.h>
#include "hooks.h"
#include "protection.h"
#include "heartbeat.h"
#include "shared_config.h"

// Глобальная конфигурация
static shared_config::SharedConfig g_config = {};
static bool g_config_loaded = false;

// Инициализация всех систем
static void InitializeSystems() {
    // 1. Инициализация защиты (ВРЕМЕННО ОТКЛЮЧЕНА - слишком агрессивная)
    protection::Initialize();
    
    // 2. Читаем конфигурацию из shared memory
    if (shared_config::ReadConfig(&g_config)) {
        g_config_loaded = true;
        
        // 3. Запускаем heartbeat если включён
        if (g_config.flags & CONFIG_FLAG_HEARTBEAT_ENABLED) {
            heartbeat::HeartbeatConfig hbConfig;
            hbConfig.server_url = g_config.server_url;
            hbConfig.license_key = g_config.license_key;
            hbConfig.hwid = g_config.hwid;
            hbConfig.product_code = g_config.product_code;
            hbConfig.event_token = g_config.event_token;
            hbConfig.interval_ms = g_config.heartbeat_interval_ms;
            
            if (hbConfig.interval_ms < 30000) {
                hbConfig.interval_ms = 60000; // Минимум 1 минута
            }
            
            heartbeat::Initialize(hbConfig);
        }
        
        // Затираем конфигурацию в памяти после использования
        SecureZeroMemory(&g_config, sizeof(g_config));
    }
    
    // 4. Инициализация хуков DirectX
    hooks::Init();
}

// Shutdown
static void ShutdownSystems() {
    // Остановка heartbeat
    heartbeat::Shutdown();
    
    // Остановка защиты (ВРЕМЕННО ОТКЛЮЧЕНА)
    // protection::Shutdown();
    
    // Остановка хуков
    hooks::Shutdown();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        
        // Запускаем инициализацию в отдельном потоке
        // чтобы не блокировать loader lock
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            // Небольшая задержка для стабильности
            Sleep(100);
            InitializeSystems();
            return 0;
        }, nullptr, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        ShutdownSystems();
    }
    return TRUE;
}
