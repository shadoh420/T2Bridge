/*
 * T2Bridge v1.0 - Tribes 2 Script Bridge DLL
 * 
 * Exposes player health to TorqueScript via file-based IPC.
 * Works by hooking the game's SetRenderPosition function to read
 * the player's damage value from memory.
 * 
 * Installation: Copy version.dll and T2Bridge.dll to GameData/ folder
 * The version.dll proxy automatically loads T2Bridge.dll on game start.
 */

#include <Windows.h>
#include <cstdio>
#include <cmath>
#include <detours.h>

#define GET_OFFSET(type, ptr, offset) *((type*)(((unsigned int)(ptr)) + (offset)))

// Forward declarations
void OnDLLProcessAttach();
void WriteDataFile();
DWORD WINAPI UpdateThread(LPVOID lpParam);

// Global state
static char g_GameDataPath[MAX_PATH] = { 0 };
static bool g_Running = true;

// Player data
static float g_PlayerHealth = 1.0f;
static bool g_PlayerValid = false;

// ============================================================================
// MEMORY OFFSETS
// ============================================================================

namespace Offsets {
    constexpr unsigned int ControlObjectFlag = 181 * 4;  // 724
    constexpr unsigned int DamageLevel = 0x7F0;          // mDamage field
}

// Hook addresses for Tribes 2 (TribesNext compatible)
namespace Addresses {
    constexpr unsigned int SetRenderPosition = 0x005D98C0;
}

// Validation offsets - these must be valid for a proper player object
namespace ValidationOffsets {
    constexpr unsigned int Offset_0x800 = 512 * 4;  // Must be 0 for valid player
    constexpr unsigned int Offset_0x26C = 0x26C;    // Must be non-zero for valid player
}

// ============================================================================
// UPDATE THREAD - Writes health data to file
// ============================================================================

static float g_LastWrittenHealth = -1.0f;
static bool g_LastWrittenValid = false;

DWORD WINAPI UpdateThread(LPVOID lpParam) {
    while (g_Running) {
        // Only write to file if health changed significantly or validity changed
        bool healthChanged = (fabs(g_PlayerHealth - g_LastWrittenHealth) > 0.005f);
        bool validChanged = (g_PlayerValid != g_LastWrittenValid);
        
        if (healthChanged || validChanged) {
            WriteDataFile();
            g_LastWrittenHealth = g_PlayerHealth;
            g_LastWrittenValid = g_PlayerValid;
        }
        
        Sleep(100);
    }
    return 0;
}

// ============================================================================
// HOOK
// ============================================================================

// Check if game window is active (avoid processing during alt-tab)
static bool IsGameWindowActive() {
    HWND hwnd = FindWindowA(NULL, "Tribes 2");
    if (hwnd == NULL) return false;
    return (GetForegroundWindow() == hwnd);
}

static int g_HookCallCount = 0;

namespace Hooks {
    typedef void(__thiscall* PlayerSetRenderPosition_t)(void*, void*, void*, void*);
    PlayerSetRenderPosition_t Original_SetRenderPosition = (PlayerSetRenderPosition_t)Addresses::SetRenderPosition;
    
    void __fastcall Hook_SetRenderPosition(void* thisPlayer, void* edx, void* arg1, void* arg2, void* arg3) {
        // ALWAYS call original FIRST - critical for game stability
        Original_SetRenderPosition(thisPlayer, arg1, arg2, arg3);
        
        g_HookCallCount++;
        
        // Only process every 100th call (reduces overhead)
        if ((g_HookCallCount % 100) != 0) {
            return;
        }
        
        // Skip processing if game window is not active (alt-tabbed)
        if (!IsGameWindowActive()) {
            g_PlayerValid = false;
            return;
        }
        
        // Reset validity - must prove valid each time
        g_PlayerValid = false;
        
        // Use SEH for all memory access
        __try {
            if (thisPlayer == nullptr) {
                return;
            }
            
            // Validate pointer range
            unsigned int ptrVal = (unsigned int)thisPlayer;
            if (ptrVal < 0x00100000 || ptrVal > 0x7FFF0000) {
                return;
            }
            
            // Validate player object: check offset 0x800 == 0 and 0x26C != 0
            int val_0x800 = GET_OFFSET(int, thisPlayer, ValidationOffsets::Offset_0x800);
            unsigned int val_0x26C = GET_OFFSET(unsigned int, thisPlayer, ValidationOffsets::Offset_0x26C);
            
            if (val_0x800 != 0 || val_0x26C == 0) {
                return;
            }
            
            // Check control flag to verify this is the local player
            void* controlFlag = GET_OFFSET(void*, thisPlayer, Offsets::ControlObjectFlag);
            if (controlFlag == nullptr) {
                return;
            }
            
            // Read damage
            float damage = GET_OFFSET(float, thisPlayer, Offsets::DamageLevel);
            
            // Validate damage value (check for NaN and range)
            if (damage != damage || damage < 0.0f || damage > 1.0f) {
                return;
            }
            
            // All checks passed
            g_PlayerValid = true;
            g_PlayerHealth = 1.0f - damage;
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            g_PlayerValid = false;
        }
    }
}

// ============================================================================
// DATA FILE OUTPUT
// ============================================================================

void WriteDataFile() {
    if (g_GameDataPath[0] == '\0') return;
    
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\t2bridge_data.txt", g_GameDataPath);
    
    FILE* f = fopen(filepath, "w");
    if (f) {
        fprintf(f, "valid=%d\n", g_PlayerValid ? 1 : 0);
        fprintf(f, "health=%.4f\n", g_PlayerHealth);
        fclose(f);
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void FindGameDataPath() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
        // Write to GameData/base/ so TorqueScript FileObject can find it
        snprintf(g_GameDataPath, MAX_PATH, "%s\\base", exePath);
        CreateDirectoryA(g_GameDataPath, NULL);
    }
}

void InstallHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)Hooks::Original_SetRenderPosition, Hooks::Hook_SetRenderPosition);
    DetourTransactionCommit();
}

void OnDLLProcessAttach() {
    FindGameDataPath();
    
    // Wait for game to initialize
    Sleep(2000);
    
    InstallHooks();
    
    // Start update thread
    CreateThread(NULL, 0, UpdateThread, NULL, 0, NULL);
    
    // Write initial data file
    WriteDataFile();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)OnDLLProcessAttach, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        g_Running = false;
        break;
    }
    return TRUE;
}
