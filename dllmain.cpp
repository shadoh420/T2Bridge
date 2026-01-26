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
#include <cstring>
#include "../Detours/include/detours.h"

#pragma comment(lib, "../Detours/lib.X86/detours.lib")

#define GET_OFFSET(type, ptr, offset) *((type*)(((unsigned int)(ptr)) + (offset)))

// Forward declarations
void OnDLLProcessAttach();
void WriteDataFile();
DWORD WINAPI UpdateThread(LPVOID lpParam);

// Global state
static char g_GameDataPath[MAX_PATH] = { 0 };
static bool g_Running = true;
static HANDLE g_Mutex = NULL;

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
    
    // Console functions (discovered via Ghidra RE)
    constexpr unsigned int Con_setVariable = 0x00426070;
    constexpr unsigned int Con_getVariable = 0x004261f0;
}

// ============================================================================
// TORQUESCRIPT CONSOLE FUNCTIONS
// ============================================================================

// Function pointer types for Torque Console functions
typedef void (__cdecl *Con_setVariable_t)(const char* varName, const char* value);
typedef const char* (__cdecl *Con_getVariable_t)(const char* varName);

// Function pointers (initialized at runtime)
static Con_setVariable_t Con_setVariable = (Con_setVariable_t)Addresses::Con_setVariable;
static Con_getVariable_t Con_getVariable = (Con_getVariable_t)Addresses::Con_getVariable;

// Test function to verify Con::setVariable works
void TestConsoleFunctions() {
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\t2bridge_console_test.txt", g_GameDataPath);
    FILE* f = fopen(filepath, "w");
    
    if (f) {
        fprintf(f, "=== T2Bridge Console Function Test ===\n\n");
        
        // Test 1: Set a variable
        fprintf(f, "Test 1: Calling Con_setVariable...\n");
        fprintf(f, "  Address: 0x%08X\n", Addresses::Con_setVariable);
        
        __try {
            Con_setVariable("$T2Bridge::TestVar", "HELLO_FROM_DLL");
            fprintf(f, "  Result: SUCCESS - No crash!\n\n");
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            fprintf(f, "  Result: CRASHED - Exception occurred!\n\n");
            fclose(f);
            return;
        }
        
        // Test 2: Read it back
        fprintf(f, "Test 2: Calling Con_getVariable...\n");
        fprintf(f, "  Address: 0x%08X\n", Addresses::Con_getVariable);
        
        __try {
            const char* result = Con_getVariable("$T2Bridge::TestVar");
            if (result != nullptr) {
                fprintf(f, "  Result: SUCCESS - Got value: \"%s\"\n\n", result);
            } else {
                fprintf(f, "  Result: Got NULL pointer\n\n");
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            fprintf(f, "  Result: CRASHED - Exception occurred!\n\n");
            fclose(f);
            return;
        }
        
        // Test 3: Set the health variable directly (no file IO needed!)
        fprintf(f, "Test 3: Setting $T2Bridge::DirectHealth...\n");
        __try {
            Con_setVariable("$T2Bridge::DirectHealth", "0.9999");
            fprintf(f, "  Result: SUCCESS!\n");
            fprintf(f, "  You can now check this in game console with: echo($T2Bridge::DirectHealth);\n\n");
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            fprintf(f, "  Result: CRASHED!\n\n");
        }
        
        fprintf(f, "=== Test Complete ===\n");
        fclose(f);
    }
}

// Validation offsets - these must be valid for a proper player object
namespace ValidationOffsets {
    constexpr unsigned int Offset_0x800 = 512 * 4;  // Must be 0 for valid player
    constexpr unsigned int Offset_0x26C = 0x26C;    // Must be non-zero for valid player
    constexpr unsigned int Namespace = 36;          // Object namespace pointer
    constexpr unsigned int TeamId = 0x18;           // Team ID within 0x26C struct
}

// ============================================================================
// UPDATE THREAD - Sets TorqueScript variables directly via Con::setVariable
// ============================================================================

static float g_LastWrittenHealth = -1.0f;
static bool g_LastWrittenValid = false;

// Update TorqueScript variables directly (no file I/O!)
void UpdateScriptVariables() {
    char healthStr[32];
    snprintf(healthStr, sizeof(healthStr), "%.4f", g_PlayerHealth);
    
    // Set variables directly in TorqueScript memory
    Con_setVariable("$T2Bridge::Health", healthStr);
    Con_setVariable("$T2Bridge::Valid", g_PlayerValid ? "1" : "0");
}

DWORD WINAPI UpdateThread(LPVOID lpParam) {
    while (g_Running) {
        WaitForSingleObject(g_Mutex, INFINITE);
        
        bool healthChanged = (fabs(g_PlayerHealth - g_LastWrittenHealth) > 0.005f);
        bool validChanged = (g_PlayerValid != g_LastWrittenValid);
        
        if (healthChanged || validChanged) {
            WriteDataFile();  // Keep file output for legacy autokit script
            // NOTE: Do NOT call UpdateScriptVariables here - it runs on background thread
            // and will race with the main thread hook. The hook sets the variables directly.
            g_LastWrittenHealth = g_PlayerHealth;
            g_LastWrittenValid = g_PlayerValid;
        }
        
        ReleaseMutex(g_Mutex);
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
    
    // Track last values to avoid redundant setVariable calls
    static float s_LastSetHealth = -1.0f;
    static bool s_LastSetValid = false;
    static int s_DebugCounter = 0;
    
    void __fastcall Hook_SetRenderPosition(void* thisPlayer, void* edx, void* arg1, void* arg2, void* arg3) {
        
        g_HookCallCount++;
        
        // Only process every Nth call to reduce overhead
        if ((g_HookCallCount % 200) == 0 && thisPlayer != nullptr) {
            
            unsigned int ptrVal = (unsigned int)thisPlayer;
            if (ptrVal >= 0x00100000 && ptrVal <= 0x7FFF0000) {
                
                int val_0x800 = GET_OFFSET(int, thisPlayer, ValidationOffsets::Offset_0x800);
                unsigned int val_0x26C = GET_OFFSET(unsigned int, thisPlayer, ValidationOffsets::Offset_0x26C);
                
                if (val_0x800 == 0 && val_0x26C != 0) {
                    void* controlFlag = GET_OFFSET(void*, thisPlayer, Offsets::ControlObjectFlag);
                    
                    if (controlFlag != nullptr) {
                        float damage = GET_OFFSET(float, thisPlayer, Offsets::DamageLevel);
                        
                        if (damage == damage && damage >= 0.0f && damage <= 1.0f) {
                            float health = 1.0f - damage;
                            bool valid = true;
                            
                            // Update global state (for legacy file output if needed)
                            WaitForSingleObject(g_Mutex, INFINITE);
                            g_PlayerValid = valid;
                            g_PlayerHealth = health;
                            ReleaseMutex(g_Mutex);
                            
                            // DIRECTLY set TorqueScript variables from the main game thread!
                            // Only update if value changed significantly
                            bool healthChanged = (fabs(health - s_LastSetHealth) > 0.005f);
                            bool validChanged = (valid != s_LastSetValid);
                            
                            if (healthChanged || validChanged) {
                                char healthStr[32];
                                snprintf(healthStr, sizeof(healthStr), "%.4f", health);
                                
                                // DEBUG: Log every call to Con_setVariable
                                s_DebugCounter++;
                                if (g_GameDataPath[0] != '\0') {
                                    char debugPath[MAX_PATH];
                                    snprintf(debugPath, MAX_PATH, "%s\\t2bridge_debug.txt", g_GameDataPath);
                                    FILE* df = fopen(debugPath, "a");
                                    if (df) {
                                        fprintf(df, "[%d] Calling Con_setVariable: $T2Bridge::Health = \"%s\"\n", s_DebugCounter, healthStr);
                                        fclose(df);
                                    }
                                }
                                
                                Con_setVariable("$T2Bridge::Health", healthStr);
                                Con_setVariable("$T2Bridge::Valid", "1");
                                s_LastSetHealth = health;
                                s_LastSetValid = valid;
                            }
                        }
                    }
                }
            }
        }
        
        Original_SetRenderPosition(thisPlayer, arg1, arg2, arg3);
    }
}

// ============================================================================
// LEGACY DATA FILE OUTPUT (kept for debugging, but no longer used)
// ============================================================================

void WriteDataFile() {
    // This function is now deprecated - we use Con::setVariable instead!
    // Keeping it for emergency fallback/debugging only
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
        // Write to GameData/ root (same folder as exe)
        strncpy(g_GameDataPath, exePath, MAX_PATH);
        g_GameDataPath[MAX_PATH - 1] = '\0';  // Ensure null termination
    }
}

void InstallHooks() {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)Hooks::Original_SetRenderPosition, Hooks::Hook_SetRenderPosition);
    LONG result = DetourTransactionCommit();
    
    // Debug: Create a marker file to indicate hook installation result
    char filepath[MAX_PATH];
    snprintf(filepath, MAX_PATH, "%s\\t2bridge_hook_status.txt", g_GameDataPath);
    FILE* f = fopen(filepath, "w");
    if (f) {
        fprintf(f, "Hook install result: %ld\n", result);
        fprintf(f, "Original ptr: 0x%p\n", Hooks::Original_SetRenderPosition);
        fclose(f);
    }
}

static HWND WaitForGameWindow(int timeoutMs) {
    int elapsed = 0;
    const int checkInterval = 100;
    
    while (elapsed < timeoutMs) {
        HWND hwnd = FindWindowA(NULL, "Tribes 2");
        if (hwnd != NULL) {
            return hwnd;
        }
        Sleep(checkInterval);
        elapsed += checkInterval;
    }
    return NULL;
}

void OnDLLProcessAttach() {
    FindGameDataPath();
    
    g_Mutex = CreateMutex(NULL, FALSE, NULL);
    HWND hwnd = WaitForGameWindow(30000);
    
    if (hwnd == NULL) {
        // Game window never appeared, don't install hooks
        char filepath[MAX_PATH];
        snprintf(filepath, MAX_PATH, "%s\\t2bridge_error.txt", g_GameDataPath);
        FILE* f = fopen(filepath, "w");
        if (f) {
            fprintf(f, "ERROR: Game window not found after 30s timeout\n");
            fclose(f);
        }
        return;
    }
    
    // Additional delay after window exists to let game fully initialize
    // The render system may not be ready immediately when the window appears
    Sleep(3000);
    
    InstallHooks();
    
    // Test the console functions we discovered via Ghidra RE
    // TODO: Remove this test in production builds
    TestConsoleFunctions();
    
    // Start update thread (now uses Con::setVariable directly!)
    CreateThread(NULL, 0, UpdateThread, NULL, 0, NULL);
    
    // Set initial variable values
    UpdateScriptVariables();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        
        // Immediate debug marker - writes to GameData folder before anything else
        {
            char markerPath[MAX_PATH];
            GetModuleFileNameA(NULL, markerPath, MAX_PATH);
            char* lastSlash = strrchr(markerPath, '\\');
            if (lastSlash) {
                strcpy_s(lastSlash + 1, MAX_PATH - (lastSlash - markerPath + 1), "T2BRIDGE_LOADED.txt");
                FILE* f = fopen(markerPath, "w");
                if (f) {
                    fprintf(f, "T2Bridge.dll loaded successfully at DLL_PROCESS_ATTACH\n");
                    fclose(f);
                }
            }
        }
        
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)OnDLLProcessAttach, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        g_Running = false;
        break;
    }
    return TRUE;
}
