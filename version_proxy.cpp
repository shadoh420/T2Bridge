/*
 * version.dll Proxy - Loads T2Bridge.dll automatically
 * 
 */

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static HMODULE g_RealVersion = NULL;

static void LoadRealVersion() {
    if (g_RealVersion) return;
    
    char systemPath[MAX_PATH];
    GetSystemDirectoryA(systemPath, MAX_PATH);
    strcat_s(systemPath, "\\version.dll");
    
    g_RealVersion = LoadLibraryA(systemPath);
}

// Use naked functions with jump tables to forward calls
#define PROXY_FUNC(name) \
    static FARPROC p_##name = NULL; \
    extern "C" __declspec(naked) void __stdcall proxy_##name() { \
        __asm { jmp p_##name } \
    }

// We'll use a simpler approach - export wrappers that get the proc address on first call

typedef BOOL (WINAPI* GetFileVersionInfoA_t)(LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI* GetFileVersionInfoW_t)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef DWORD (WINAPI* GetFileVersionInfoSizeA_t)(LPCSTR, LPDWORD);
typedef DWORD (WINAPI* GetFileVersionInfoSizeW_t)(LPCWSTR, LPDWORD);
typedef BOOL (WINAPI* VerQueryValueA_t)(LPCVOID, LPCSTR, LPVOID*, PUINT);
typedef BOOL (WINAPI* VerQueryValueW_t)(LPCVOID, LPCWSTR, LPVOID*, PUINT);
typedef BOOL (WINAPI* GetFileVersionInfoExA_t)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI* GetFileVersionInfoExW_t)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
typedef DWORD (WINAPI* GetFileVersionInfoSizeExA_t)(DWORD, LPCSTR, LPDWORD);
typedef DWORD (WINAPI* GetFileVersionInfoSizeExW_t)(DWORD, LPCWSTR, LPDWORD);

static GetFileVersionInfoA_t p_GetFileVersionInfoA = NULL;
static GetFileVersionInfoW_t p_GetFileVersionInfoW = NULL;
static GetFileVersionInfoSizeA_t p_GetFileVersionInfoSizeA = NULL;
static GetFileVersionInfoSizeW_t p_GetFileVersionInfoSizeW = NULL;
static VerQueryValueA_t p_VerQueryValueA = NULL;
static VerQueryValueW_t p_VerQueryValueW = NULL;
static GetFileVersionInfoExA_t p_GetFileVersionInfoExA = NULL;
static GetFileVersionInfoExW_t p_GetFileVersionInfoExW = NULL;
static GetFileVersionInfoSizeExA_t p_GetFileVersionInfoSizeExA = NULL;
static GetFileVersionInfoSizeExW_t p_GetFileVersionInfoSizeExW = NULL;

static void LoadFunctions() {
    if (p_GetFileVersionInfoA) return;
    LoadRealVersion();
    if (!g_RealVersion) return;
    
    p_GetFileVersionInfoA = (GetFileVersionInfoA_t)GetProcAddress(g_RealVersion, "GetFileVersionInfoA");
    p_GetFileVersionInfoW = (GetFileVersionInfoW_t)GetProcAddress(g_RealVersion, "GetFileVersionInfoW");
    p_GetFileVersionInfoSizeA = (GetFileVersionInfoSizeA_t)GetProcAddress(g_RealVersion, "GetFileVersionInfoSizeA");
    p_GetFileVersionInfoSizeW = (GetFileVersionInfoSizeW_t)GetProcAddress(g_RealVersion, "GetFileVersionInfoSizeW");
    p_VerQueryValueA = (VerQueryValueA_t)GetProcAddress(g_RealVersion, "VerQueryValueA");
    p_VerQueryValueW = (VerQueryValueW_t)GetProcAddress(g_RealVersion, "VerQueryValueW");
    p_GetFileVersionInfoExA = (GetFileVersionInfoExA_t)GetProcAddress(g_RealVersion, "GetFileVersionInfoExA");
    p_GetFileVersionInfoExW = (GetFileVersionInfoExW_t)GetProcAddress(g_RealVersion, "GetFileVersionInfoExW");
    p_GetFileVersionInfoSizeExA = (GetFileVersionInfoSizeExA_t)GetProcAddress(g_RealVersion, "GetFileVersionInfoSizeExA");
    p_GetFileVersionInfoSizeExW = (GetFileVersionInfoSizeExW_t)GetProcAddress(g_RealVersion, "GetFileVersionInfoSizeExW");
}

extern "C" {
    __declspec(dllexport) BOOL WINAPI Proxy_GetFileVersionInfoA(LPCSTR a, DWORD b, DWORD c, LPVOID d) {
        LoadFunctions();
        return p_GetFileVersionInfoA ? p_GetFileVersionInfoA(a, b, c, d) : FALSE;
    }
    
    __declspec(dllexport) BOOL WINAPI Proxy_GetFileVersionInfoW(LPCWSTR a, DWORD b, DWORD c, LPVOID d) {
        LoadFunctions();
        return p_GetFileVersionInfoW ? p_GetFileVersionInfoW(a, b, c, d) : FALSE;
    }
    
    __declspec(dllexport) DWORD WINAPI Proxy_GetFileVersionInfoSizeA(LPCSTR a, LPDWORD b) {
        LoadFunctions();
        return p_GetFileVersionInfoSizeA ? p_GetFileVersionInfoSizeA(a, b) : 0;
    }
    
    __declspec(dllexport) DWORD WINAPI Proxy_GetFileVersionInfoSizeW(LPCWSTR a, LPDWORD b) {
        LoadFunctions();
        return p_GetFileVersionInfoSizeW ? p_GetFileVersionInfoSizeW(a, b) : 0;
    }
    
    __declspec(dllexport) BOOL WINAPI Proxy_VerQueryValueA(LPCVOID a, LPCSTR b, LPVOID* c, PUINT d) {
        LoadFunctions();
        return p_VerQueryValueA ? p_VerQueryValueA(a, b, c, d) : FALSE;
    }
    
    __declspec(dllexport) BOOL WINAPI Proxy_VerQueryValueW(LPCVOID a, LPCWSTR b, LPVOID* c, PUINT d) {
        LoadFunctions();
        return p_VerQueryValueW ? p_VerQueryValueW(a, b, c, d) : FALSE;
    }
    
    __declspec(dllexport) BOOL WINAPI Proxy_GetFileVersionInfoExA(DWORD a, LPCSTR b, DWORD c, DWORD d, LPVOID e) {
        LoadFunctions();
        return p_GetFileVersionInfoExA ? p_GetFileVersionInfoExA(a, b, c, d, e) : FALSE;
    }
    
    __declspec(dllexport) BOOL WINAPI Proxy_GetFileVersionInfoExW(DWORD a, LPCWSTR b, DWORD c, DWORD d, LPVOID e) {
        LoadFunctions();
        return p_GetFileVersionInfoExW ? p_GetFileVersionInfoExW(a, b, c, d, e) : FALSE;
    }
    
    __declspec(dllexport) DWORD WINAPI Proxy_GetFileVersionInfoSizeExA(DWORD a, LPCSTR b, LPDWORD c) {
        LoadFunctions();
        return p_GetFileVersionInfoSizeExA ? p_GetFileVersionInfoSizeExA(a, b, c) : 0;
    }
    
    __declspec(dllexport) DWORD WINAPI Proxy_GetFileVersionInfoSizeExW(DWORD a, LPCWSTR b, LPDWORD c) {
        LoadFunctions();
        return p_GetFileVersionInfoSizeExW ? p_GetFileVersionInfoSizeExW(a, b, c) : 0;
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        
        // Load T2Bridge.dll from the same directory
        {
            char dllPath[MAX_PATH];
            GetModuleFileNameA(hModule, dllPath, MAX_PATH);
            
            char* lastSlash = strrchr(dllPath, '\\');
            if (lastSlash) {
                strcpy_s(lastSlash + 1, MAX_PATH - (lastSlash - dllPath + 1), "T2Bridge.dll");
                LoadLibraryA(dllPath);
            }
        }
        break;
        
    case DLL_PROCESS_DETACH:
        if (g_RealVersion) {
            FreeLibrary(g_RealVersion);
            g_RealVersion = NULL;
        }
        break;
    }
    return TRUE;
}
