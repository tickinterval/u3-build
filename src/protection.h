#pragma once

#include <windows.h>

namespace protection {

// ================== ANTI-DEBUG ==================

// Скрыть текущий поток от отладчика
void HideFromDebugger();

// Базовые проверки отладчика
bool IsDebuggerPresent_Check();
bool CheckRemoteDebugger();
bool CheckPEB();
bool CheckHardwareBreakpoints();
bool CheckNtGlobalFlag();
bool CheckDebugFlags();
bool CheckDebugObjectHandle();

// Комплексная проверка
bool IsDebuggerDetected();

// Проверка на подозрительные процессы
bool CheckSuspiciousProcesses();

// ================== ANTI-DUMP ==================

// Удаление PE заголовков из памяти
void ErasePEHeader();

// Подмена PE заголовков (мусор)
void CorruptPEHeader();

// Скрытие модуля из PEB
void UnlinkFromPEB();

// ================== WATCHDOG ==================

// Запуск фонового потока защиты
void StartProtectionThread();

// Остановка потока защиты
void StopProtectionThread();

// ================== INTEGRITY ==================

// Проверка хуков в критических функциях
bool CheckApiHooks();

// ================== INITIALIZATION ==================

// Инициализация всей защиты
void Initialize();

// Деинициализация
void Shutdown();

} // namespace protection


