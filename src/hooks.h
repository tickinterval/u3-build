#pragma once
#include <windows.h>
#include <d3d11.h>

namespace hooks {

// Initialize the hooks (creates thread, finds window, etc.)
void Init();

// Clean up hooks (restore wndproc, etc.)
void Shutdown();

}
